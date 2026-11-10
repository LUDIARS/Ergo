#include "ks_export.h"

#include <algorithm>
#include <cmath>

namespace ergo::studio::plugins::vfx_suggest {
namespace {

double num(const JsonValue& cfg, const char* a, const char* b) {
    const JsonValue* x = field(cfg, a);
    const JsonValue* y = x ? field(*x, b) : nullptr;
    return y ? y->as_number() : 0;
}

double at(const JsonValue& cfg, const char* a, const char* b, std::size_t i) {
    const JsonValue* x = field(cfg, a);
    const JsonValue* y = x ? field(*x, b) : nullptr;
    return y && y->is_array() && i < y->a->size() ? (*y->a)[i].as_number() : 0;
}

JsonValue constant(double v) { return jobj({{"mode", jnum(0)}, {"min", jnum(v)}, {"max", jnum(v)}}); }

JsonValue two_consts(double lo, double hi) {
    if (std::fabs(hi - lo) < 1e-6) return constant(lo);
    return jobj({{"mode", jnum(1)}, {"min", jnum(lo)}, {"max", jnum(hi)}});
}

JsonValue linear_curve(double v0, double v1) {
    JsonValue keys = JsonValue::make_array();
    for (auto [t, v] : {std::pair<double, double>{0, v0}, {1, v1}}) {
        JsonValue k = JsonValue::make_array();
        k.push(jnum(t));
        k.push(jnum(v));
        keys.push(std::move(k));
    }
    return jobj({{"keys", keys}});
}

/// v0 (birth) -> v1 (death) as a Curve-mode MinMaxCurve.
JsonValue fade(double v0, double v1) {
    if (std::fabs(v0 - v1) < 1e-6) return constant(v0);
    return jobj({{"mode", jnum(2)}, {"min", jnum(std::min(v0, v1))}, {"max", jnum(std::max(v0, v1))},
                 {"curve_min", linear_curve(v0, v1)}});
}

JsonValue arr(std::initializer_list<double> xs) {
    JsonValue a = JsonValue::make_array();
    for (double x : xs) a.push(jnum(x));
    return a;
}

} // namespace

JsonValue to_ks_emitter_json(const JsonValue& cfg, const std::string& key) {
    const JsonValue* name   = field(cfg, "name");
    const JsonValue* render = field(cfg, "render");
    const bool additive     = render && field(*render, "blend") && field(*render, "blend")->as_string() == "additive";
    return jobj({
        {"name", jstr(key.empty() ? (name ? name->as_string() : std::string()) : key)},
        {"duration", jnum(0)},
        {"loop", jbool(true)},
        {"start_lifetime", two_consts(num(cfg, "initial", "lifetimeMin"), num(cfg, "initial", "lifetimeMax"))},
        {"start_speed", two_consts(num(cfg, "initial", "speedMin"), num(cfg, "initial", "speedMax"))},
        {"start_size", constant(num(cfg, "initial", "size"))},
        {"start_color", arr({at(cfg, "initial", "color", 0), at(cfg, "initial", "color", 1), at(cfg, "initial", "color", 2),
                             at(cfg, "initial", "color", 3)})},
        {"max_particles", jnum(num(cfg, "emission", "maxAlive"))},
        {"simulation_space", jnum(0)},
        {"gravity_modifier", jnum(at(cfg, "forces", "gravity", 1) / 9.81)},
        {"rate_over_time", constant(num(cfg, "emission", "rate"))},
        {"shape", jnum(1)},
        {"sphere_radius", jnum(num(cfg, "initial", "positionRadius"))},
        {"cone_radius", jnum(num(cfg, "initial", "positionRadius"))},
        {"cone_angle_deg", jnum(num(cfg, "initial", "velocityAngleSpreadDeg") / 2)},
        {"shape_scale", arr({1, 1, 1})},
        {"size_over_lifetime", fade(num(cfg, "overLife", "sizeStart"), num(cfg, "overLife", "sizeEnd"))},
        {"color_over_lifetime",
         jobj({{"r", linear_curve(at(cfg, "overLife", "colorStart", 0), at(cfg, "overLife", "colorEnd", 0))},
               {"g", linear_curve(at(cfg, "overLife", "colorStart", 1), at(cfg, "overLife", "colorEnd", 1))},
               {"b", linear_curve(at(cfg, "overLife", "colorStart", 2), at(cfg, "overLife", "colorEnd", 2))},
               {"a", linear_curve(at(cfg, "overLife", "colorStart", 3), at(cfg, "overLife", "colorEnd", 3))}})},
        {"blend_mode", jnum(additive ? 2 : 0)},
    });
}

} // namespace ergo::studio::plugins::vfx_suggest
