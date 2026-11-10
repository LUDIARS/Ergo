#pragma once
/// Built-in VFX presets (port of vfx-suggest/catalog.ts, data in
/// catalog.json embedded at build time) and the game ids the generator knows.
#include "../common/json_util.h"

#include <string>
#include <vector>

namespace ergo::studio::plugins::vfx_suggest {

struct VfxPreset {
    std::string name;
    std::string rationale;
    JsonValue   config;   ///< ParticleEffectConfig
};
JsonValue to_json_value(const VfxPreset& p);

/// "ks" / "ac" / "ul" / "generic".
bool                     is_game_id(const std::string& id);
const std::vector<std::pair<std::string, std::string>>& game_labels();   ///< (id, label) in order

/// Every catalog preset, in file order.
const std::vector<VfxPreset>& catalog();

/// Up to `n` presets for few-shot prompting: the game's hinted ones first,
/// then the rest of the catalog, without duplicates.
std::vector<VfxPreset> catalog_examples(const std::string& game, std::size_t n);

} // namespace ergo::studio::plugins::vfx_suggest
