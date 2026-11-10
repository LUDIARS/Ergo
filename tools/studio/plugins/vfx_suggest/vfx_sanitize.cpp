#include "vfx_sanitize.h"

#include "../particle/particle_schema.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ergo::studio::plugins::vfx_suggest {
namespace {

using particle::find_range;

const JsonValue kEmpty = JsonValue::make_object();
const JsonValue& sub(const JsonValue& v, const char* k) {
    const JsonValue* x = field(v, k);
    return x && x->is_object() ? *x : kEmpty;
}

/// JS Number(v): numbers as is, numeric strings parsed, anything else NaN.
double js_number(const JsonValue* v) {
    if (!v) return std::nan("");
    if (v->is_number()) return v->n;
    if (v->is_bool()) return v->b ? 1 : 0;
    if (v->is_string()) {
        const std::string& s = v->s;
        const auto b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return 0;   // Number("") === 0
        char* e = nullptr;
        const double d = std::strtod(s.c_str() + b, &e);
        const auto rest = std::string(e).find_first_not_of(" \t\r\n");
        return rest == std::string::npos ? d : std::nan("");
    }
    return std::nan("");
}

double clamp(const JsonValue* v, double lo, double hi, double def) {
    const double n = js_number(v);
    return std::isfinite(n) ? std::min(hi, std::max(lo, n)) : def;
}

double within(const JsonValue* v, const char* key, double def) {
    const auto* r = find_range(key);
    return clamp(v, r->lo, r->hi, def);
}

double def_at(const char* section, const char* key) {
    static const JsonValue d = particle::default_effect();
    return field(*field(d, section), key)->as_number();
}

JsonValue def_arr(const char* section, const char* key) {
    static const JsonValue d = particle::default_effect();
    return deep_copy(*field(*field(d, section), key));
}

JsonValue rgba(const JsonValue* c, const JsonValue& def) {
    if (!c || !c->is_array() || c->a->size() < 4) return deep_copy(def);
    JsonValue out = JsonValue::make_array();
    for (std::size_t i = 0; i < 4; ++i) out.push(jnum(clamp(&(*c->a)[i], 0, 1, (*def.a)[i].n)));
    return out;
}

} // namespace

JsonValue sanitize_config(const JsonValue& raw) {
    const JsonValue& c   = raw.is_object() ? raw : kEmpty;
    const JsonValue& em  = sub(c, "emission");
    const JsonValue& in  = sub(c, "initial");
    const JsonValue& ol  = sub(c, "overLife");
    const JsonValue& fo  = sub(c, "forces");
    const JsonValue& re  = sub(c, "render");
    const JsonValue  dg  = def_arr("forces", "gravity");
    const JsonValue* gr  = field(fo, "gravity");
    const JsonValue& grav = gr && gr->is_array() ? *gr : dg;
    auto g = [&](std::size_t i) { return i < grav.a->size() ? &(*grav.a)[i] : nullptr; };

    const JsonValue* name = field(c, "name");
    auto I = [&](const char* k) { return within(field(in, k), (std::string("initial.") + k).c_str(), def_at("initial", k)); };

    return jobj({
        {"version", jnum(particle::kSchemaVersion)},
        {"name", jstr(name && name->is_string() && !name->s.empty() ? name->s : std::string("untitled"))},
        {"emission", jobj({{"rate", jnum(within(field(em, "rate"), "emission.rate", def_at("emission", "rate")))},
                           {"maxAlive", jnum(within(field(em, "maxAlive"), "emission.maxAlive", def_at("emission", "maxAlive")))}})},
        {"initial", jobj({{"positionShape", jstr("disc")},
                          {"svgSource", jstr("")},
                          {"svgScale", jnum(1)},
                          {"positionRadius", jnum(I("positionRadius"))},
                          {"velocityAngleDeg", jnum(I("velocityAngleDeg"))},
                          {"velocityAngleSpreadDeg", jnum(I("velocityAngleSpreadDeg"))},
                          {"speedMin", jnum(I("speedMin"))},
                          {"speedMax", jnum(I("speedMax"))},
                          {"lifetimeMin", jnum(I("lifetimeMin"))},
                          {"lifetimeMax", jnum(I("lifetimeMax"))},
                          {"size", jnum(I("size"))},
                          {"color", rgba(field(in, "color"), def_arr("initial", "color"))}})},
        {"overLife", jobj({{"sizeStart", jnum(within(field(ol, "sizeStart"), "overLife.sizeStart", 1))},
                           {"sizeEnd", jnum(within(field(ol, "sizeEnd"), "overLife.sizeEnd", 0))},
                           {"colorStart", rgba(field(ol, "colorStart"), def_arr("overLife", "colorStart"))},
                           {"colorEnd", rgba(field(ol, "colorEnd"), def_arr("overLife", "colorEnd"))},
                           {"velocityDamping", jnum(within(field(ol, "velocityDamping"), "overLife.velocityDamping",
                                                           def_at("overLife", "velocityDamping")))}})},
        {"forces", jobj({{"gravity", [&] {
                              JsonValue a = JsonValue::make_array();
                              a.push(jnum(within(g(0), "forces.gravity.x", (*dg.a)[0].n)));
                              a.push(jnum(within(g(1), "forces.gravity.y", (*dg.a)[1].n)));
                              return a;
                          }()}})},
        {"render", jobj({{"blend", jstr(field(re, "blend") && field(re, "blend")->as_string() == "alpha" ? "alpha" : "additive")},
                         {"shape", jstr(field(re, "shape") && field(re, "shape")->as_string() == "square" ? "square" : "circle")}})},
    });
}

std::vector<VfxPreset> sanitize_presets(const JsonValue& raw) {
    std::vector<VfxPreset> out;
    if (!raw.is_array()) return out;
    for (std::size_t i = 0; i < raw.a->size() && i < 4; ++i) {
        const JsonValue& item = (*raw.a)[i];
        const JsonValue& obj  = item.is_object() ? item : kEmpty;
        const JsonValue* name = field(obj, "name");
        const JsonValue* why  = field(obj, "rationale");
        const JsonValue* cfg  = field(obj, "config");
        out.push_back({name && name->is_string() ? name->s : "preset-" + std::to_string(i + 1),
                       why && why->is_string() ? why->s : std::string(),
                       sanitize_config(cfg && !cfg->is_null() ? *cfg : obj)});
    }
    return out;
}

std::string extract_json(const std::string& text) {
    auto trim = [](const std::string& s) {
        const auto b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return std::string();
        return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
    };
    const auto fence = text.find("```");
    if (fence != std::string::npos) {
        std::size_t body = fence + 3;
        if (text.compare(body, 4, "json") == 0) body += 4;
        const auto close = text.find("```", body);
        if (close != std::string::npos) return trim(text.substr(body, close - body));
    }
    const auto first = text.find('{');
    const auto last  = text.rfind('}');
    if (first != std::string::npos && last != std::string::npos && last > first) return text.substr(first, last - first + 1);
    return trim(text);
}

} // namespace ergo::studio::plugins::vfx_suggest
