#include "particle_schema.h"

#include <initializer_list>

namespace ergo::studio::plugins::particle {
namespace {

JsonValue jarr(std::initializer_list<double> xs) {
    JsonValue a = JsonValue::make_array();
    for (double x : xs) a.push(jnum(x));
    return a;
}

} // namespace

JsonValue default_effect() {
    return jobj({
        {"version", jnum(kSchemaVersion)},
        {"name", jstr("untitled")},
        {"emission", jobj({{"rate", jnum(60)}, {"maxAlive", jnum(400)}})},
        {"initial", jobj({{"positionShape", jstr("disc")},
                          {"svgSource", jstr("")},
                          {"svgScale", jnum(1)},
                          {"positionRadius", jnum(4)},
                          {"velocityAngleDeg", jnum(270)},
                          {"velocityAngleSpreadDeg", jnum(60)},
                          {"speedMin", jnum(80)},
                          {"speedMax", jnum(180)},
                          {"lifetimeMin", jnum(0.7)},
                          {"lifetimeMax", jnum(1.4)},
                          {"size", jnum(8)},
                          {"color", jarr({1, 0.75, 0.25, 1})}})},
        {"overLife", jobj({{"sizeStart", jnum(1.0)},
                           {"sizeEnd", jnum(0.0)},
                           {"colorStart", jarr({1, 0.75, 0.25, 1})},
                           {"colorEnd", jarr({0.7, 0.1, 0.0, 0})},
                           {"velocityDamping", jnum(0.6)}})},
        {"forces", jobj({{"gravity", jarr({0, 120})}})},
        {"render", jobj({{"blend", jstr("additive")}, {"shape", jstr("circle")}})},
    });
}

JsonValue merge_config(const JsonValue& base, const JsonValue& partial) {
    JsonValue out = deep_copy(base);
    if (out.is_object() && partial.is_object()) {
        for (auto& [k, cur] : *out.o) {
            const JsonValue* p = partial.find(k);
            if (!p || p->is_null()) continue;
            if (p->is_object() && cur.is_object()) {
                for (const auto& [pk, pv] : *p->o) cur.set(pk, deep_copy(pv));
            } else {
                cur = deep_copy(*p);
            }
        }
    }
    if (out.is_object()) out.set("version", jnum(kSchemaVersion));
    return out;
}

const std::vector<ValueRange>& particle_ranges() {
    static const std::vector<ValueRange> kRanges = {
        {"emission.rate", 1, 400},
        {"emission.maxAlive", 1, 2000},
        {"initial.positionRadius", 0, 256},
        {"initial.velocityAngleDeg", 0, 360},
        {"initial.velocityAngleSpreadDeg", 0, 360},
        {"initial.speedMin", 0, 600},
        {"initial.speedMax", 0, 600},
        {"initial.lifetimeMin", 0.05, 6},
        {"initial.lifetimeMax", 0.05, 6},
        {"initial.size", 0, 64},
        {"overLife.sizeStart", 0, 8},
        {"overLife.sizeEnd", 0, 8},
        {"overLife.velocityDamping", 0, 1},
        {"forces.gravity.x", -600, 600},
        {"forces.gravity.y", -600, 600},
    };
    return kRanges;
}

const ValueRange* find_range(const std::string& key) {
    for (const auto& r : particle_ranges()) {
        if (key == r.key) return &r;
    }
    return nullptr;
}

} // namespace ergo::studio::plugins::particle
