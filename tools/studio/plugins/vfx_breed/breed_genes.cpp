#include "breed_genes.h"

#include "../particle/particle_schema.h"

namespace ergo::studio::plugins::vfx_breed {
namespace {

GeneSpec num(const char* key, const char* label, GeneScale scale = GeneScale::Linear, bool wrap = false,
             bool integer = false) {
    const auto* r = particle::find_range(key);
    GeneSpec g;
    g.id      = key;
    g.label   = label;
    g.lo      = r->lo;
    g.hi      = r->hi;
    g.scale   = scale;
    g.wrap    = wrap;
    g.integer = integer;
    g.key     = key;
    return g;
}

void color(std::vector<GeneSpec>& out, const char* field, const char* label) {
    static const struct {
        const char* suffix;
        const char* label;
        bool        wrap;
    } kChannels[] = {{"hue", "hue", true}, {"saturation", "saturation", false}, {"value", "brightness", false},
                     {"alpha", "opacity", false}};
    for (int i = 0; i < 4; ++i) {
        GeneSpec g;
        g.id       = std::string(field) + "." + kChannels[i].suffix;
        g.label    = std::string(label) + " " + kChannels[i].label;
        g.wrap     = kChannels[i].wrap;
        g.is_color = true;
        g.key      = field;
        g.channel  = i;
        out.push_back(std::move(g));
    }
}

} // namespace

const std::vector<GeneSpec>& genes() {
    static const std::vector<GeneSpec> v = [] {
        std::vector<GeneSpec> g{
            num("emission.rate", "particles emitted per second", GeneScale::Log),
            num("emission.maxAlive", "max particles alive at once", GeneScale::Log, false, true),
            num("initial.positionRadius", "spawn disc radius (px)"),
            num("initial.velocityAngleDeg", "launch direction (deg, 270 = up)", GeneScale::Linear, true),
            num("initial.velocityAngleSpreadDeg", "launch cone width (deg)"),
            num("initial.speedMin", "min launch speed (px/s)"),
            num("initial.speedMax", "max launch speed (px/s)"),
            num("initial.lifetimeMin", "min lifetime (s)"),
            num("initial.lifetimeMax", "max lifetime (s)"),
            num("initial.size", "particle size (px)"),
            num("overLife.sizeStart", "size multiplier at birth"),
            num("overLife.sizeEnd", "size multiplier at death"),
            num("overLife.velocityDamping", "velocity kept per second (1 = no drag)"),
            num("forces.gravity.x", "horizontal force (px/s^2)"),
            num("forces.gravity.y", "vertical force (px/s^2, + = down)"),
        };
        color(g, "initial.color", "spawn color");
        color(g, "overLife.colorStart", "start color");
        color(g, "overLife.colorEnd", "end color");
        return g;
    }();
    return v;
}

int gene_index(const std::string& id) {
    const auto& g = genes();
    for (std::size_t i = 0; i < g.size(); ++i) {
        if (g[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

} // namespace ergo::studio::plugins::vfx_breed
