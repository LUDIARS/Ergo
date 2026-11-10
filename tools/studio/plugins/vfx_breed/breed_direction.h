#pragma once
/// Direction hint -> per-gene bias (port of vfx-breed/direction.ts). The LLM
/// never writes config values: it only says which genes to push and which
/// way (-1..1); this module asks that question and turns the answer into a
/// bias vector aligned with genes().
#include "../common/json_util.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::vfx_breed {

using DirectionWeights = std::map<std::string, double>;

std::string build_direction_prompt(const std::string& direction, const JsonValue& current);

/// {"weights": {...}} or a bare {...}; unknown genes and non-numbers dropped.
/// nullopt when the reply holds no JSON object.
std::optional<DirectionWeights> parse_weights(const std::string& text);

/// Clamped to -1..1, 0 for genes without a weight.
std::vector<double> to_bias(const DirectionWeights& w);

JsonValue to_json_value(const DirectionWeights& w);

} // namespace ergo::studio::plugins::vfx_breed
