#pragma once
/// ParticleEffectConfig <-> Genome (port of vfx-breed/genome.ts and color.ts).
/// decode() always yields values inside the shared particle ranges.
#include "../common/json_util.h"

#include <array>
#include <string>
#include <vector>

namespace ergo::studio::plugins::vfx_breed {

struct Genome {
    std::vector<double> genes;   ///< aligned with genes(); each in [0, 1]
    std::string         blend = "additive";   ///< "additive" | "alpha"
    std::string         shape = "circle";     ///< "circle" | "square"
};

using Rgba = std::array<double, 4>;
Rgba rgba_to_hsva(const Rgba& c);
Rgba hsva_to_rgba(const Rgba& c);

/// Wrap circular genes into [0, 1), clamp the others; non-finite -> 0.
double fold(double v, bool wrap);

/// Any partial / foreign JSON merged with the default effect.
JsonValue to_config(const JsonValue& raw);

Genome    encode(const JsonValue& raw);
/// `carrier` supplies positionShape / svgSource / svgScale (inherited, never bred).
JsonValue decode(const Genome& g, const JsonValue& carrier, const std::string& name);

} // namespace ergo::studio::plugins::vfx_breed
