#pragma once

#include "ergo/gpu_particle/emitter_descriptor.h"
#include "ergo/gpu_particle/gpu_backend.h"
#include "ergo/gpu_particle/types.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ergo::gpu_particle {

class IGpuBackend;

// ---------------------------------------------------------------------------
// ParticleSystem — owns every registered emitter and drives the per-frame
// spawn/update compute dispatch.
//
// Workflow:
//   1. Construct + `initialize(backend, cfg)`.
//   2. For each emitter, `create_emitter(desc)` returns a handle.
//      The system allocates per-emitter GPU buffers and bakes the curves.
//   3. `set_world_transform(handle, world)` whenever the emitter moves.
//   4. `update(dt)` every frame — runs spawn + update kernels.
//   5. `get_instance_buffer(handle)` gives the rendering layer the
//      per-live-particle data (position / size / color / rotation /
//      atlas frame). `get_live_particle_count(handle)` returns the
//      instance count hint (copied from the counter SSBO).
//   6. Destroy emitters that are no longer needed with
//      `destroy_emitter(handle)` and eventually `shutdown()`.
// ---------------------------------------------------------------------------

struct ParticleSystemConfig {
    /// Precompiled SPIR-V for `particle_spawn.comp`. If null the system
    /// falls back to a CPU simulation path (useful for headless unit
    /// tests) instead of touching the backend.
    const uint32_t* spawn_shader_spirv      = nullptr;
    size_t          spawn_shader_spirv_size = 0;

    const uint32_t* update_shader_spirv      = nullptr;
    size_t          update_shader_spirv_size = 0;

    /// Workgroup X size used by the compute kernels. Must match the
    /// `layout(local_size_x = …)` declaration in the shaders.
    uint32_t workgroup_size = 64;

    /// When the backend isn't provided (e.g. tests) or shaders are
    /// missing, the system runs a conservative CPU-side emulation using
    /// the same descriptor math. The resulting state is available via
    /// the introspection API even though no GPU buffers exist.
    bool allow_cpu_fallback = true;
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&)            = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    /// Take ownership of the backend reference (the backend itself must
    /// outlive the particle system). Passing nullptr engages the
    /// CPU-fallback path when `allow_cpu_fallback` is true.
    bool initialize(IGpuBackend* backend, const ParticleSystemConfig& cfg);
    void shutdown();

    bool is_initialized() const;

    // ── Emitter registration ────────────────────────────────
    EmitterHandle create_emitter(const EmitterDescriptor& desc,
                                 std::string* error = nullptr);
    void          destroy_emitter(EmitterHandle);

    // ── Runtime control ─────────────────────────────────────
    void play(EmitterHandle);
    void pause(EmitterHandle);
    void stop(EmitterHandle, bool clear_particles = true);
    void set_world_transform(EmitterHandle, const Float4x4& world);
    void set_emitter_position(EmitterHandle, Vec3f world_position);

    // ── Per-frame ───────────────────────────────────────────
    void update(float delta_time);

    // ── Read-back for rendering ─────────────────────────────
    /// GPU SSBO containing the per-live-particle `InstanceData` stream.
    /// Returns an empty handle on CPU fallback.
    BufferHandle get_instance_buffer(EmitterHandle) const;

    /// Live particle count. On GPU mode this reads back from the counters
    /// buffer (can stall — callers that drive indirect draws should use
    /// `get_counters_buffer` instead).
    uint32_t     get_live_particle_count(EmitterHandle) const;

    /// Raw counters buffer (alive_count / spawn_cursor / draw_count /
    /// total_emitted). Suitable for `vkCmdDrawIndirectCount` style paths.
    BufferHandle get_counters_buffer(EmitterHandle) const;

    /// Descriptor inspection (returns nullptr if handle unknown).
    const EmitterDescriptor* get_descriptor(EmitterHandle) const;
    EmitterState              get_state(EmitterHandle) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ergo::gpu_particle
