#pragma once

/// ParticleEffectConfig — mirror of particle-editor's wire schema (v1).
///
/// Field names and units match the JSON shape produced by particle-editor
/// (see particle-editor/src/schema.ts). Hosts typically receive raw JSON
/// over WebSocket, decode into this struct, and pass it to ParticleSystem
/// via `set_config`.

#include <array>
#include <cstdint>
#include <string>

namespace ergo::particle {

constexpr int SCHEMA_VERSION = 1;

enum class BlendMode : uint8_t { Additive = 0, Alpha = 1 };
enum class ShapeMode : uint8_t { Circle   = 0, Square = 1 };

struct ParticleEffectConfig {
    int             version  = SCHEMA_VERSION;
    std::string     name     = "untitled";

    // emission
    float           emission_rate      = 60.0f;
    int             emission_max_alive = 400;

    // initial
    float           init_position_radius        = 4.0f;
    float           init_velocity_angle_deg     = 270.0f;
    float           init_velocity_spread_deg    = 60.0f;
    float           init_speed_min              = 80.0f;
    float           init_speed_max              = 180.0f;
    float           init_lifetime_min           = 0.7f;
    float           init_lifetime_max           = 1.4f;
    float           init_size                   = 8.0f;
    std::array<float, 4> init_color             = {1.0f, 0.75f, 0.25f, 1.0f};

    // overLife
    float           life_size_start       = 1.0f;
    float           life_size_end         = 0.0f;
    std::array<float, 4> life_color_start = {1.0f, 0.75f, 0.25f, 1.0f};
    std::array<float, 4> life_color_end   = {0.7f, 0.1f, 0.0f, 0.0f};
    float           life_velocity_damping = 0.6f; // multiplied per second

    // forces
    std::array<float, 2> gravity = {0.0f, 120.0f};

    // render
    BlendMode       render_blend = BlendMode::Additive;
    ShapeMode       render_shape = ShapeMode::Circle;
};

/// Parse a JSON document (as produced by particle-editor's `state` message
/// or `/api/effect`) into a ParticleEffectConfig. Returns false on syntax
/// error or missing top-level object. Unknown fields are ignored; missing
/// fields keep the existing values in `out` (deep-merge semantics).
bool parse_config_json(const std::string& json, ParticleEffectConfig& out);

} // namespace ergo::particle
