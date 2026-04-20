#pragma once

#include "ergo/gpu_particle/curve.h"
#include "ergo/gpu_particle/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ergo::gpu_particle {

// ---------------------------------------------------------------------------
// Emission bursts. Shuriken-style discrete emission events: at `time`
// (seconds into the emitter), emit `count_min..count_max` particles, and
// repeat `cycles` times every `interval` seconds.
// ---------------------------------------------------------------------------

struct Burst {
    float    time     = 0.0f;
    uint32_t count_min = 0;
    uint32_t count_max = 0;
    uint32_t cycles   = 1;
    float    interval = 0.01f;
    float    probability = 1.0f;  // 0..1 chance this burst fires
};

// ---------------------------------------------------------------------------
// Collision plane (Shuriken "World" planar collider).
// The update shader reflects particles about this plane, subtracting
// `lifetime_loss` on contact so particles can fizzle out over impacts.
// ---------------------------------------------------------------------------

struct CollisionPlane {
    Vec3f normal   = {0.f, 1.f, 0.f};   // outward-facing
    float distance = 0.f;               // plane eq: n·p + d = 0
    float bounce   = 0.5f;              // 0 = absorb, 1 = perfect rebound
    float damp     = 0.1f;              // tangential velocity damp 0..1
    float lifetime_loss = 0.f;          // seconds subtracted on impact
    bool  enabled  = false;
};

// ---------------------------------------------------------------------------
// Wind zone — uniform directional wind modulated by time-varying pulse.
// ---------------------------------------------------------------------------

struct WindZone {
    Vec3f direction       = {1.f, 0.f, 0.f};
    float main_strength   = 0.f;
    float turbulence      = 0.f;
    float pulse_frequency = 0.5f;
    float pulse_magnitude = 0.f;
    bool  enabled         = false;
};

// ---------------------------------------------------------------------------
// Emitter descriptor
//
// Fully describes an emitter. All parameters mirror Unity Shuriken module
// sections ("Main", "Emission", "Shape", "*-over-Lifetime", "Collision",
// "External Forces", "Renderer"). A descriptor is intended to be copied
// into the `ParticleSystem`, which then builds the GPU-side state.
// ---------------------------------------------------------------------------

struct EmitterDescriptor {
    // ── Identity ────────────────────────────────────────────
    std::string name;

    // ── Main ────────────────────────────────────────────────
    float        duration        = 5.0f;   // 0 = infinite
    bool         loop            = true;
    bool         prewarm         = false;  // simulate `duration` seconds on play
    float        start_delay     = 0.0f;
    MinMaxCurve  start_lifetime  = MinMaxCurve::constant(5.0f);
    MinMaxCurve  start_speed     = MinMaxCurve::constant(5.0f);
    MinMaxCurve  start_size      = MinMaxCurve::constant(1.0f);
    MinMaxCurve  start_rotation  = MinMaxCurve::constant(0.0f);   // radians
    Vec4f        start_color     = {1.f, 1.f, 1.f, 1.f};
    uint32_t     max_particles   = 1000;
    SimulationSpace simulation_space = SimulationSpace::World;
    float        simulation_speed = 1.0f;
    uint32_t     random_seed     = 0;      // 0 → auto-seed on create

    // ── Emission ────────────────────────────────────────────
    MinMaxCurve           rate_over_time     = MinMaxCurve::constant(10.0f);
    MinMaxCurve           rate_over_distance = MinMaxCurve::constant(0.0f);
    std::vector<Burst>    bursts;

    // ── Shape ───────────────────────────────────────────────
    EmitterShape shape               = EmitterShape::Cone;
    Vec3f        shape_position      = {0.f, 0.f, 0.f};
    Vec3f        shape_rotation_deg  = {0.f, 0.f, 0.f};
    Vec3f        shape_scale         = {1.f, 1.f, 1.f};
    // Cone
    float        cone_angle_deg      = 25.f;
    float        cone_radius         = 1.f;
    float        cone_radius_thickness = 1.f;   // 0 = rim only, 1 = full
    // Sphere / hemisphere / circle
    float        sphere_radius       = 1.f;
    // Box
    Vec3f        box_extents         = {1.f, 1.f, 1.f};
    // Edge
    float        edge_length         = 1.f;

    // ── Over-lifetime modifiers ─────────────────────────────
    // Velocity: one curve per axis (world or local per simulation_space).
    Curve        velocity_over_lifetime_x;
    Curve        velocity_over_lifetime_y;
    Curve        velocity_over_lifetime_z;
    // Size multiplier (applied to start_size). Default = constant 1.
    MinMaxCurve  size_over_lifetime      = MinMaxCurve::constant(1.0f);
    // Angular velocity (radians / second). Positive = CCW around view axis.
    MinMaxCurve  rotation_over_lifetime  = MinMaxCurve::constant(0.0f);
    // Color over lifetime: 4 curves (RGBA). Each curve's domain is 0..1 age.
    Curve        color_r_over_lifetime;
    Curve        color_g_over_lifetime;
    Curve        color_b_over_lifetime;
    Curve        color_a_over_lifetime;

    // ── Physics / external forces ───────────────────────────
    Vec3f                        gravity          = {0.f, -9.81f, 0.f};
    float                        gravity_modifier = 0.f;   // scales gravity
    float                        linear_damping   = 0.f;   // per-second
    std::vector<CollisionPlane>  collision_planes;
    WindZone                     wind;

    // ── Rendering ───────────────────────────────────────────
    RenderStyle  render_style        = RenderStyle::Billboard;
    BlendMode    blend_mode          = BlendMode::Alpha;
    uint64_t     texture_handle      = 0;   // backend-specific (0 = untextured)
    uint64_t     mesh_handle         = 0;   // used when render_style == Mesh
    uint32_t     atlas_cols          = 1;
    uint32_t     atlas_rows          = 1;
    Curve        atlas_frame_over_lifetime;  // 0 .. (cols*rows - 1)
    bool         stretch_use_velocity = true;
    float        stretch_scale        = 1.0f;

    // ── Helpers ─────────────────────────────────────────────

    /// Reject obvious misconfigurations. `error` receives a human-readable
    /// message on failure. Returns true if the descriptor is usable.
    bool validate(std::string* error = nullptr) const;
};

} // namespace ergo::gpu_particle
