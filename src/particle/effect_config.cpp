#include "ergo/particle/effect_config.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace ergo::particle {

// ---------------------------------------------------------------------------
// Tiny JSON walker. Just enough to deep-merge the editor's config payload
// without bringing in a full parser. Anything we don't recognize is ignored.
// ---------------------------------------------------------------------------
namespace {

struct Cur {
    const std::string& s;
    std::size_t i = 0;
    bool err = false;

    bool eof() const { return i >= s.size(); }
    char peek() const { return s[i]; }

    void skip_ws() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i; else break;
        }
    }

    bool match(char c) { skip_ws(); if (eof() || peek() != c) return false; ++i; return true; }

    bool parse_string(std::string& out) {
        if (!match('"')) { err = true; return false; }
        out.clear();
        while (!eof()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (eof()) { err = true; return false; }
                char e = s[i++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'u':
                        if (s.size() - i < 4) { err = true; return false; }
                        i += 4; // not decoded (we don't need non-ASCII)
                        break;
                    default: err = true; return false;
                }
            } else out.push_back(c);
        }
        err = true; return false;
    }

    bool parse_number(double& out) {
        skip_ws();
        std::size_t start = i;
        if (!eof() && (peek() == '-' || peek() == '+')) ++i;
        while (!eof() && (std::isdigit(static_cast<unsigned char>(peek())) ||
                          peek() == '.' || peek() == 'e' || peek() == 'E' ||
                          peek() == '-' || peek() == '+')) ++i;
        if (start == i) { err = true; return false; }
        char* endp = nullptr;
        out = std::strtod(s.c_str() + start, &endp);
        return endp != s.c_str() + start;
    }

    bool parse_bool(bool& out) {
        skip_ws();
        if (s.compare(i, 4, "true") == 0)  { i += 4; out = true;  return true; }
        if (s.compare(i, 5, "false") == 0) { i += 5; out = false; return true; }
        err = true; return false;
    }

    /// Skip any value (number, string, bool, null, object, array).
    bool skip_value() {
        skip_ws();
        if (eof()) { err = true; return false; }
        char c = peek();
        if (c == '"') { std::string d; return parse_string(d); }
        if (c == '{') return skip_object();
        if (c == '[') return skip_array();
        if (c == 't' || c == 'f') { bool b; return parse_bool(b); }
        if (c == 'n') {
            if (s.compare(i, 4, "null") == 0) { i += 4; return true; }
            err = true; return false;
        }
        double d; return parse_number(d);
    }

    bool skip_object() {
        if (!match('{')) { err = true; return false; }
        if (match('}')) return true;
        while (true) {
            skip_ws();
            std::string k;
            if (!parse_string(k)) return false;
            if (!match(':')) { err = true; return false; }
            if (!skip_value()) return false;
            skip_ws();
            if (match(',')) continue;
            if (match('}')) return true;
            err = true; return false;
        }
    }

    bool skip_array() {
        if (!match('[')) { err = true; return false; }
        if (match(']')) return true;
        while (true) {
            if (!skip_value()) return false;
            skip_ws();
            if (match(',')) continue;
            if (match(']')) return true;
            err = true; return false;
        }
    }
};

bool parse_number_array(Cur& c, float* out, int n) {
    if (!c.match('[')) return false;
    for (int idx = 0; idx < n; ++idx) {
        if (idx > 0) {
            c.skip_ws();
            if (!c.match(',')) return false;
        }
        double d;
        if (!c.parse_number(d)) return false;
        out[idx] = static_cast<float>(d);
    }
    c.skip_ws();
    if (!c.match(']')) return false;
    return true;
}

bool apply_section(Cur& c, ParticleEffectConfig& out, const std::string& section);

bool apply_object(Cur& c, ParticleEffectConfig& out) {
    if (!c.match('{')) return false;
    if (c.match('}')) return true;
    while (true) {
        c.skip_ws();
        std::string k;
        if (!c.parse_string(k)) return false;
        if (!c.match(':')) return false;
        c.skip_ws();
        if (k == "version") {
            double d; if (!c.parse_number(d)) return false; out.version = static_cast<int>(d);
        } else if (k == "name") {
            std::string s; if (!c.parse_string(s)) return false; out.name = s;
        } else if (k == "emission" || k == "initial" || k == "overLife" ||
                   k == "forces"   || k == "render") {
            if (!apply_section(c, out, k)) return false;
        } else {
            if (!c.skip_value()) return false;
        }
        c.skip_ws();
        if (c.match(',')) continue;
        if (c.match('}')) return true;
        return false;
    }
}

bool apply_section(Cur& c, ParticleEffectConfig& out, const std::string& section) {
    if (!c.match('{')) return false;
    if (c.match('}')) return true;
    while (true) {
        c.skip_ws();
        std::string k;
        if (!c.parse_string(k)) return false;
        if (!c.match(':')) return false;
        c.skip_ws();

        bool consumed = false;
        if (section == "emission") {
            double d;
            if (k == "rate") { if (!c.parse_number(d)) return false; out.emission_rate = static_cast<float>(d); consumed = true; }
            else if (k == "maxAlive") { if (!c.parse_number(d)) return false; out.emission_max_alive = static_cast<int>(d); consumed = true; }
        }
        else if (section == "initial") {
            double d;
            if      (k == "positionRadius")          { if (!c.parse_number(d)) return false; out.init_position_radius     = static_cast<float>(d); consumed = true; }
            else if (k == "velocityAngleDeg")        { if (!c.parse_number(d)) return false; out.init_velocity_angle_deg  = static_cast<float>(d); consumed = true; }
            else if (k == "velocityAngleSpreadDeg") { if (!c.parse_number(d)) return false; out.init_velocity_spread_deg = static_cast<float>(d); consumed = true; }
            else if (k == "speedMin")                { if (!c.parse_number(d)) return false; out.init_speed_min           = static_cast<float>(d); consumed = true; }
            else if (k == "speedMax")                { if (!c.parse_number(d)) return false; out.init_speed_max           = static_cast<float>(d); consumed = true; }
            else if (k == "lifetimeMin")             { if (!c.parse_number(d)) return false; out.init_lifetime_min        = static_cast<float>(d); consumed = true; }
            else if (k == "lifetimeMax")             { if (!c.parse_number(d)) return false; out.init_lifetime_max        = static_cast<float>(d); consumed = true; }
            else if (k == "size")                    { if (!c.parse_number(d)) return false; out.init_size                = static_cast<float>(d); consumed = true; }
            else if (k == "color")                   { if (!parse_number_array(c, out.init_color.data(), 4)) return false; consumed = true; }
        }
        else if (section == "overLife") {
            double d;
            if      (k == "sizeStart")        { if (!c.parse_number(d)) return false; out.life_size_start        = static_cast<float>(d); consumed = true; }
            else if (k == "sizeEnd")          { if (!c.parse_number(d)) return false; out.life_size_end          = static_cast<float>(d); consumed = true; }
            else if (k == "colorStart")       { if (!parse_number_array(c, out.life_color_start.data(), 4)) return false; consumed = true; }
            else if (k == "colorEnd")         { if (!parse_number_array(c, out.life_color_end.data(),   4)) return false; consumed = true; }
            else if (k == "velocityDamping")  { if (!c.parse_number(d)) return false; out.life_velocity_damping  = static_cast<float>(d); consumed = true; }
        }
        else if (section == "forces") {
            if (k == "gravity") { if (!parse_number_array(c, out.gravity.data(), 2)) return false; consumed = true; }
        }
        else if (section == "render") {
            std::string s;
            if (k == "blend") { if (!c.parse_string(s)) return false; out.render_blend = (s == "alpha") ? BlendMode::Alpha : BlendMode::Additive; consumed = true; }
            else if (k == "shape") { if (!c.parse_string(s)) return false; out.render_shape = (s == "square") ? ShapeMode::Square : ShapeMode::Circle; consumed = true; }
        }

        if (!consumed) {
            if (!c.skip_value()) return false;
        }

        c.skip_ws();
        if (c.match(',')) continue;
        if (c.match('}')) return true;
        return false;
    }
}

} // namespace

bool parse_config_json(const std::string& json, ParticleEffectConfig& out) {
    Cur c{json};
    c.skip_ws();
    if (c.eof()) return false;
    if (!apply_object(c, out)) return false;
    return !c.err;
}

} // namespace ergo::particle
