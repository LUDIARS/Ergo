#pragma once
/// ParticleEffectConfig -> GPU EmitterDescriptor JSON read by the KS game's
/// VfxCatalog::register_from_json (the ergo::shuriken_migrator output shape).
/// Port of vfx-suggest/ks_export.ts:
///   - 2D gravity y (down positive) becomes gravity_modifier = y / 9.81
///   - EmitterShape Sphere (1) with the 2D positionRadius
///   - BlendMode additive -> 2, alpha -> 0
#include "../common/json_util.h"

#include <string>

namespace ergo::studio::plugins::vfx_suggest {

/// `cfg` must be a full config (merge_config with the defaults first).
/// `key` empty = the config's name.
JsonValue to_ks_emitter_json(const JsonValue& cfg, const std::string& key);

} // namespace ergo::studio::plugins::vfx_suggest
