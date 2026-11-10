#pragma once
/// Clamp raw LLM output into valid ParticleEffectConfig presets (port of
/// vfx-suggest/sanitize.ts) against particle_schema's range table.
#include "vfx_catalog.h"

#include <string>
#include <vector>

namespace ergo::studio::plugins::vfx_suggest {

/// One config: every numeric field clamped to its range (default when not a
/// finite number), colours to 0..1, positionShape "disc" / svgSource "" /
/// svgScale 1 filled in, blend / shape restricted to their two values.
JsonValue sanitize_config(const JsonValue& raw);

/// At most 4 presets; a missing name becomes "preset-<n>"; an item without
/// "config" is read as the config itself.
std::vector<VfxPreset> sanitize_presets(const JsonValue& raw);

/// JSON text inside LLM output: a ```json fence, else the outermost {...}.
std::string extract_json(const std::string& text);

} // namespace ergo::studio::plugins::vfx_suggest
