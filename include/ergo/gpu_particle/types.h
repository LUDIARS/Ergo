#pragma once

#include <cstdint>

namespace ergo::gpu_particle {

// ---------------------------------------------------------------------------
// Scalar / vector types
//
// Ergo modules are standalone — we don't pull in Pictor math headers here.
// Layouts are chosen to match GLSL std430 when packed contiguously
// (vec3 / vec4 alignment rules are respected by the SoA particle struct
// in particle_state.h).
// ---------------------------------------------------------------------------

struct Vec2f {
    float x = 0.f, y = 0.f;
};

struct Vec3f {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct Vec4f {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
};

struct Quat {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;  // identity
    static Quat identity() { return {}; }
};

/// 4x4 row-major matrix (row-vector convention — `v * M`, translation at
/// `m[3][0..2]`). Chosen to match Pictor's convention so world transforms
/// can be handed over without conversion.
struct Float4x4 {
    float m[4][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };
    static Float4x4 identity() { return {}; }
    void set_translation(float x, float y, float z) {
        m[3][0] = x; m[3][1] = y; m[3][2] = z;
    }
};

// ---------------------------------------------------------------------------
// Handles
// ---------------------------------------------------------------------------

using EmitterHandle = uint64_t;
constexpr EmitterHandle INVALID_EMITTER = 0;

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class EmitterShape : uint8_t {
    Point       = 0,
    Sphere      = 1,
    Hemisphere  = 2,
    Cone        = 3,
    Box         = 4,
    Circle      = 5,
    Edge        = 6,
};

enum class SimulationSpace : uint8_t {
    World = 0,  // particles detach from emitter transform after spawn
    Local = 1,  // particles follow emitter transform every frame
};

enum class RenderStyle : uint8_t {
    Billboard            = 0,   // camera-facing quad
    StretchedBillboard   = 1,   // elongated along velocity
    HorizontalBillboard  = 2,   // aligned to world XZ plane
    VerticalBillboard    = 3,   // aligned to world Y axis
    Mesh                 = 4,   // instance a supplied mesh
};

enum class BlendMode : uint8_t {
    Alpha       = 0,   // straight alpha
    Premultiplied = 1,
    Additive    = 2,
    Multiply    = 3,
};

/// Execution state of an emitter instance.
enum class EmitterState : uint8_t {
    Stopped  = 0,
    Playing  = 1,
    Paused   = 2,
};

} // namespace ergo::gpu_particle
