#pragma once
/// ParticleEffectConfig v1 as JSON (port of tools/ergo particle/schema.ts and
/// ranges.ts). Keep in sync with ergo_particle's ParticleEffectConfig.
#include "../common/json_util.h"

#include <string>
#include <vector>

namespace ergo::studio::plugins::particle {

constexpr int kSchemaVersion = 1;

JsonValue default_effect();

/// Merge `partial` into a copy of `base`: for every top-level key of `base`,
/// a non-null object in `partial` is merged one level deep, any other
/// non-null value replaces it (arrays wholesale). Keys `base` lacks are
/// ignored. "version" is always kSchemaVersion.
JsonValue merge_config(const JsonValue& base, const JsonValue& partial);

/// Value range of a numeric field, by dotted path ("emission.rate",
/// "forces.gravity.x", ...). Colors are 0..1 per channel and not listed.
struct ValueRange {
    const char* key;
    double      lo;
    double      hi;
};
const std::vector<ValueRange>& particle_ranges();
/// nullptr for an unknown key.
const ValueRange* find_range(const std::string& key);

} // namespace ergo::studio::plugins::particle
