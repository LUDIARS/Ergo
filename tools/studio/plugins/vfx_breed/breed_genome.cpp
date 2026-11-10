#include "breed_genome.h"

#include "../particle/particle_schema.h"
#include "breed_genes.h"
#include "breed_rng.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ergo::studio::plugins::vfx_breed {
namespace {

double clamp01(double v) { return std::min(1.0, std::max(0.0, v)); }

double normalise(const GeneSpec& s, double v) {
    const double x = std::min(s.hi, std::max(s.lo, std::isfinite(v) ? v : s.lo));
    if (s.scale == GeneScale::Log) return clamp01(std::log(x / s.lo) / std::log(s.hi / s.lo));
    return s.hi > s.lo ? clamp01((x - s.lo) / (s.hi - s.lo)) : 0;
}

double denormalise(const GeneSpec& s, double g) {
    const double t   = clamp01(g);
    const double raw = s.scale == GeneScale::Log ? s.lo * std::pow(s.hi / s.lo, t) : s.lo + (s.hi - s.lo) * t;
    const double r   = s.integer ? js_round(raw) : js_round(raw * 1000) / 1000;
    return std::min(s.hi, std::max(s.lo, r));
}

const JsonValue* at(const JsonValue& cfg, const std::string& group, const std::string& key) {
    const JsonValue* g = field(cfg, group);
    return g ? field(*g, key) : nullptr;
}

double read_number(const JsonValue& cfg, const std::string& key) {
    if (key == "forces.gravity.x" || key == "forces.gravity.y") {
        const JsonValue* g = at(cfg, "forces", "gravity");
        const std::size_t i = key.back() == 'x' ? 0 : 1;
        return g && g->is_array() && i < g->a->size() ? (*g->a)[i].as_number(std::nan("")) : std::nan("");
    }
    const auto dot = key.find('.');
    const JsonValue* v = at(cfg, key.substr(0, dot), key.substr(dot + 1));
    return v && v->is_number() ? v->n : std::nan("");
}

Rgba read_color(const JsonValue& cfg, const std::string& field_key) {
    const auto dot = field_key.find('.');
    const JsonValue* c = at(cfg, field_key.substr(0, dot), field_key.substr(dot + 1));
    Rgba out{0, 0, 0, 0};
    for (std::size_t i = 0; i < 4; ++i) {
        const double v = c && c->is_array() && i < c->a->size() ? (*c->a)[i].as_number(std::nan("")) : std::nan("");
        out[i] = std::isfinite(v) ? clamp01(v) : 0;   // JS: clamp01(Number(x)) of NaN is NaN -> folded to 0 later
    }
    return out;
}

JsonValue arr(std::initializer_list<double> xs) {
    JsonValue a = JsonValue::make_array();
    for (double x : xs) a.push(jnum(x));
    return a;
}

} // namespace

Rgba rgba_to_hsva(const Rgba& c) {
    const double r = c[0], g = c[1], b = c[2];
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    double h = 0;
    if (d > 0) {
        if (mx == r) h = std::fmod((g - b) / d, 6.0);
        else if (mx == g) h = (b - r) / d + 2;
        else h = (r - g) / d + 4;
        h /= 6;
        if (h < 0) h += 1;
    }
    return {h, mx > 0 ? d / mx : 0, mx, c[3]};
}

Rgba hsva_to_rgba(const Rgba& c) {
    const double hh = std::fmod(std::fmod(c[0], 1.0) + 1, 1.0) * 6;
    const double i  = std::floor(hh);
    const double f  = hh - i;
    const double v = c[2], s = c[1];
    const double p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (static_cast<int>(i)) {
        case 0: return {v, t, p, c[3]};
        case 1: return {q, v, p, c[3]};
        case 2: return {p, v, t, c[3]};
        case 3: return {p, q, v, c[3]};
        case 4: return {t, p, v, c[3]};
        default: return {v, p, q, c[3]};
    }
}

double fold(double v, bool wrap) {
    if (!std::isfinite(v)) return 0;
    return wrap ? std::fmod(std::fmod(v, 1.0) + 1, 1.0) : clamp01(v);
}

JsonValue to_config(const JsonValue& raw) { return particle::merge_config(particle::default_effect(), raw); }

Genome encode(const JsonValue& raw) {
    const JsonValue cfg = to_config(raw);
    std::map<std::string, Rgba> hsv;
    Genome out;
    for (const auto& s : genes()) {
        if (!s.is_color) {
            out.genes.push_back(normalise(s, read_number(cfg, s.key)));
            continue;
        }
        auto it = hsv.find(s.key);
        if (it == hsv.end()) it = hsv.emplace(s.key, rgba_to_hsva(read_color(cfg, s.key))).first;
        out.genes.push_back(fold(it->second[static_cast<std::size_t>(s.channel)], s.wrap));
    }
    const JsonValue* render = field(cfg, "render");
    out.blend = render && field(*render, "blend") && field(*render, "blend")->as_string() == "alpha" ? "alpha" : "additive";
    out.shape = render && field(*render, "shape") && field(*render, "shape")->as_string() == "square" ? "square" : "circle";
    return out;
}

JsonValue decode(const Genome& g, const JsonValue& carrier, const std::string& name) {
    const JsonValue base = to_config(carrier);
    std::map<std::string, double> nums;
    std::map<std::string, Rgba>   colors;
    const auto& spec = genes();
    for (std::size_t i = 0; i < spec.size(); ++i) {
        const double v = i < g.genes.size() ? g.genes[i] : 0;
        if (!spec[i].is_color) nums[spec[i].key] = denormalise(spec[i], v);
        else colors[spec[i].key][static_cast<std::size_t>(spec[i].channel)] = fold(v, spec[i].wrap);
    }
    auto n    = [&](const char* k) { return nums.at(k); };
    auto rgba = [&](const char* f) {
        const Rgba c = hsva_to_rgba(colors[f]);
        return arr({js_round(clamp01(c[0]) * 1000) / 1000, js_round(clamp01(c[1]) * 1000) / 1000,
                    js_round(clamp01(c[2]) * 1000) / 1000, js_round(clamp01(c[3]) * 1000) / 1000});
    };
    const double s0 = std::min(n("initial.speedMin"), n("initial.speedMax"));
    const double s1 = std::max(n("initial.speedMin"), n("initial.speedMax"));
    const double l0 = std::min(n("initial.lifetimeMin"), n("initial.lifetimeMax"));
    const double l1 = std::max(n("initial.lifetimeMin"), n("initial.lifetimeMax"));
    const JsonValue& bi = *field(base, "initial");

    return jobj({
        {"version", jnum(particle::kSchemaVersion)},
        {"name", jstr(name)},
        {"emission", jobj({{"rate", jnum(n("emission.rate"))}, {"maxAlive", jnum(n("emission.maxAlive"))}})},
        {"initial", jobj({{"positionShape", *field(bi, "positionShape")},
                          {"svgSource", *field(bi, "svgSource")},
                          {"svgScale", *field(bi, "svgScale")},
                          {"positionRadius", jnum(n("initial.positionRadius"))},
                          {"velocityAngleDeg", jnum(n("initial.velocityAngleDeg"))},
                          {"velocityAngleSpreadDeg", jnum(n("initial.velocityAngleSpreadDeg"))},
                          {"speedMin", jnum(s0)},
                          {"speedMax", jnum(s1)},
                          {"lifetimeMin", jnum(l0)},
                          {"lifetimeMax", jnum(l1)},
                          {"size", jnum(n("initial.size"))},
                          {"color", rgba("initial.color")}})},
        {"overLife", jobj({{"sizeStart", jnum(n("overLife.sizeStart"))},
                           {"sizeEnd", jnum(n("overLife.sizeEnd"))},
                           {"colorStart", rgba("overLife.colorStart")},
                           {"colorEnd", rgba("overLife.colorEnd")},
                           {"velocityDamping", jnum(n("overLife.velocityDamping"))}})},
        {"forces", jobj({{"gravity", arr({n("forces.gravity.x"), n("forces.gravity.y")})}})},
        {"render", jobj({{"blend", jstr(g.blend)}, {"shape", jstr(g.shape)}})},
    });
}

} // namespace ergo::studio::plugins::vfx_breed
