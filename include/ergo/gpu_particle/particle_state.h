#pragma once

#include "ergo/gpu_particle/types.h"

#include <cstdint>

namespace ergo::gpu_particle {

// ---------------------------------------------------------------------------
// Particle — per-particle state laid out to match the GLSL compute
// shaders' SSBO (see shaders/particle_spawn.comp and particle_update.comp).
//
// 64 bytes per particle. Fields grouped so the GLSL struct can use
// `vec3 + float` packing under std430 without hidden padding.
//
//   offset  0: vec3  position
//   offset 12: float lifetime_remaining
//   offset 16: vec3  velocity
//   offset 28: float lifetime_initial
//   offset 32: vec4  color (r,g,b,a)
//   offset 48: float size
//   offset 52: float rotation          (radians, around the view axis for
//                                        billboards; around Y for horizontal)
//   offset 56: float atlas_frame       (flipbook frame index, fractional)
//   offset 60: float seed              (per-particle PRNG state seed)
//
// The alive/dead state is encoded by `lifetime_remaining <= 0`.
// ---------------------------------------------------------------------------

struct Particle {
    Vec3f position;
    float lifetime_remaining = 0.f;
    Vec3f velocity;
    float lifetime_initial = 0.f;
    Vec4f color;
    float size = 1.f;
    float rotation = 0.f;
    float atlas_frame = 0.f;
    float seed = 0.f;
};

static_assert(sizeof(Particle) == 64, "Particle must pack to 64B for GLSL std430 compatibility");

// ---------------------------------------------------------------------------
// Counters — a single global block of atomic counters owned by the
// spawn/update shaders. Stored as uvec4 so the shader can update via
// atomicAdd() on a single binding.
//
//   alive_count:     number of particles currently alive (write target of
//                    update shader's compaction pass).
//   spawn_cursor:    index the spawn shader writes the next new particle to.
//                    Reset to 0 at the start of each frame on the CPU.
//   draw_count:      number of instances to draw this frame (mirrors
//                    alive_count after the update pass). Used as the
//                    instance count for indirect draw calls.
//   total_emitted:   cumulative counter for statistics / debugging.
// ---------------------------------------------------------------------------

struct ParticleCounters {
    uint32_t alive_count     = 0;
    uint32_t spawn_cursor    = 0;
    uint32_t draw_count      = 0;
    uint32_t total_emitted   = 0;
};

static_assert(sizeof(ParticleCounters) == 16, "ParticleCounters must match GLSL uvec4");

// ---------------------------------------------------------------------------
// InstanceData — packed per-live-particle record the update shader emits
// for the rendering backend to consume. Kept smaller than `Particle` so
// the draw-side SSBO is cache-friendly.
//
// 48 bytes, std430-compatible.
// ---------------------------------------------------------------------------

struct InstanceData {
    Vec3f position;       //  0
    float size;           // 12
    Vec4f color;          // 16
    float rotation;       // 32
    float atlas_frame;    // 36
    float _pad0;          // 40
    float _pad1;          // 44
};

static_assert(sizeof(InstanceData) == 48, "InstanceData must pack to 48B for GLSL std430");

} // namespace ergo::gpu_particle
