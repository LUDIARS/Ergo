#pragma once
/// The gene table (port of vfx-breed/genes.ts): which ParticleEffectConfig
/// values breed and how each maps to a normalised 0..1 gene. Numeric ranges
/// come from particle_schema's range table; colours breed as HSV + alpha
/// (hue wraps); blend / shape are the discrete genes (genome); positionShape /
/// svgSource / svgScale are inherited, never bred.
#include <string>
#include <vector>

namespace ergo::studio::plugins::vfx_breed {

enum class GeneScale { Linear, Log };

struct GeneSpec {
    std::string id;       ///< stable; the key the advisor answers with
    std::string label;    ///< shown to the advisor LLM
    double      lo = 0, hi = 1;
    GeneScale   scale   = GeneScale::Linear;
    bool        wrap    = false;
    bool        integer = false;
    // source
    bool        is_color = false;
    std::string key;      ///< number: dotted config key; color: "initial.color" / "overLife.colorStart" / ...
    int         channel = 0;   ///< color: 0 hue, 1 saturation, 2 value, 3 alpha
};

const std::vector<GeneSpec>& genes();
/// Index of a gene id, -1 when unknown.
int gene_index(const std::string& id);

} // namespace ergo::studio::plugins::vfx_breed
