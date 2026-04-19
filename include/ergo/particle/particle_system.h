#pragma once

/// ParticleSystem — CPU-side particle simulation.
///
/// Pure-data API. Owns the particle pool, runs emission / forces / lifetime
/// each tick, exposes the per-particle state for an external renderer
/// (typically `ergo::particle::Renderer`, but anything that can read floats).
///
/// Thread model:
///   * `set_config()` is safe to call from any thread (mutex protected;
///     the next `update()` snapshots the new config).
///   * `update()` and instance access (`particles()`, `count()`, etc.) must
///     be called from a single owner thread (typically the render thread).

#include "ergo/particle/effect_config.h"

#include <cstdint>
#include <mutex>
#include <random>
#include <vector>

namespace ergo::particle {

/// Per-particle state exposed for rendering.
struct ParticleInstance {
    float pos[3]   = {0, 0, 0};
    float size     = 0.0f;
    float color[4] = {1, 1, 1, 1};
};

class ParticleSystem {
public:
    ParticleSystem();

    /// Replace the active config. Thread-safe.
    void set_config(const ParticleEffectConfig& cfg);

    /// Snapshot of the active config. Thread-safe.
    ParticleEffectConfig config() const;

    /// Step the simulation by `dt` seconds. Single-thread.
    void update(float dt);

    /// Spawn `n` particles immediately, ignoring the rate accumulator
    /// (still bounded by `emission_max_alive`). Single-thread.
    void burst(int n);

    /// Discard all live particles. Single-thread.
    void reset();

    /// Read-only access to renderer-facing instances. Refreshed at the end
    /// of each `update()`; right after `burst()` it lags one frame behind
    /// `count()` until the next `update()` rebuilds it.
    const std::vector<ParticleInstance>& instances() const { return instances_; }

    /// Number of live particles (always up-to-date).
    std::size_t count() const { return live_.size(); }

    /// Diagnostics.
    struct Stats {
        std::size_t alive       = 0;
        uint64_t    spawned     = 0; // cumulative since construction
        uint64_t    expired     = 0;
    };
    Stats stats() const { return stats_; }

private:
    struct Particle {
        float pos[3];
        float vel[3];
        float age;
        float lifetime;
        float init_size;
        float init_color[4];
    };

    void emit_one();

    mutable std::mutex            cfg_mtx_;
    ParticleEffectConfig          cfg_;
    bool                          cfg_dirty_ = false;

    ParticleEffectConfig          active_cfg_; // local snapshot used by update()

    std::vector<Particle>         live_;
    std::vector<ParticleInstance> instances_; // rebuilt each frame for renderer
    float                         emit_accum_ = 0.0f;
    Stats                         stats_;

    std::mt19937                  rng_;
};

} // namespace ergo::particle
