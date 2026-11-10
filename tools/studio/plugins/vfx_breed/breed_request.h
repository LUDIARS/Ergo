#pragma once
/// Validation of POST /vfx-breed/api/generation bodies (port of
/// vfx-breed/request.ts):
///   { parents?: Config[], selected?: number[], direction?: string,
///     strength?: number, seed?: number, count?: number, generation?: number }
/// `selected` picks parents by index (duplicates dropped); no parent at all
/// -> the default effect.
#include "../common/json_util.h"

#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace ergo::studio::plugins::vfx_breed {

constexpr int         kCountMin          = 4;
constexpr int         kCountMax          = 16;
constexpr int         kCountDefault      = 8;
constexpr double      kStrengthDefault   = 0.15;
constexpr std::size_t kDirectionMaxChars = 500;   ///< UTF-16 units in TS; code points here

struct GenerationRequest {
    std::vector<JsonValue> parents;
    std::string            direction;
    double                 strength   = kStrengthDefault;   ///< 0..1
    double                 seed       = 0;                  ///< uint32 range
    int                    count      = kCountDefault;      ///< 4..16
    int                    generation = 0;
};

/// The request, or the error message for HTTP 400.
std::variant<GenerationRequest, std::string> parse_generation_request(const JsonValue& body,
                                                                      const std::function<uint32_t()>& seed_source);

} // namespace ergo::studio::plugins::vfx_breed
