#include "ergo/particle/particle_system.h"

#include <algorithm>
#include <cmath>

namespace ergo::particle {

namespace {
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
} // namespace

ParticleSystem::ParticleSystem()
    : rng_(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this) ^ 0xC1A5DEEDu)) {
    active_cfg_ = cfg_;
}

void ParticleSystem::set_config(const ParticleEffectConfig& cfg) {
    std::lock_guard<std::mutex> lk(cfg_mtx_);
    cfg_ = cfg;
    cfg_dirty_ = true;
}

ParticleEffectConfig ParticleSystem::config() const {
    std::lock_guard<std::mutex> lk(cfg_mtx_);
    return cfg_;
}

void ParticleSystem::reset() {
    live_.clear();
    instances_.clear();
    emit_accum_ = 0.0f;
    stats_.alive = 0;
}

void ParticleSystem::burst(int n) {
    for (int i = 0; i < n; ++i) {
        if (static_cast<int>(live_.size()) >= active_cfg_.emission_max_alive) break;
        emit_one();
    }
    stats_.alive = live_.size();
}

void ParticleSystem::update(float dt) {
    // Pull in any pending config without holding the lock during simulation.
    {
        std::lock_guard<std::mutex> lk(cfg_mtx_);
        if (cfg_dirty_) { active_cfg_ = cfg_; cfg_dirty_ = false; }
    }
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f; // clamp huge deltas

    // ---- Emission ----
    emit_accum_ += active_cfg_.emission_rate * dt;
    while (emit_accum_ >= 1.0f) {
        emit_accum_ -= 1.0f;
        if (static_cast<int>(live_.size()) >= active_cfg_.emission_max_alive) break;
        emit_one();
    }

    // ---- Forces / lifetime ----
    const float damp = std::pow(std::max(0.0001f, active_cfg_.life_velocity_damping), dt);
    const float gx   = active_cfg_.gravity[0];
    const float gy   = active_cfg_.gravity[1];

    for (std::size_t i = 0; i < live_.size(); ) {
        Particle& p = live_[i];
        p.age += dt;
        if (p.age >= p.lifetime) {
            // Swap with last and pop — O(1) removal.
            if (i + 1 != live_.size()) live_[i] = live_.back();
            live_.pop_back();
            ++stats_.expired;
            continue;
        }
        // Damping then gravity (in canvas space gravity uses +Y).
        p.vel[0] = p.vel[0] * damp + gx * dt;
        p.vel[1] = p.vel[1] * damp + gy * dt;
        p.vel[2] = p.vel[2] * damp;
        p.pos[0] += p.vel[0] * dt;
        p.pos[1] += p.vel[1] * dt;
        p.pos[2] += p.vel[2] * dt;
        ++i;
    }

    // ---- Build per-frame instance buffer for renderer ----
    instances_.clear();
    instances_.reserve(live_.size());
    for (const Particle& p : live_) {
        const float t       = (p.lifetime > 0.0f) ? clamp01(p.age / p.lifetime) : 0.0f;
        const float sizeMul = lerp(active_cfg_.life_size_start, active_cfg_.life_size_end, t);
        ParticleInstance inst;
        inst.pos[0] = p.pos[0]; inst.pos[1] = p.pos[1]; inst.pos[2] = p.pos[2];
        inst.size   = p.init_size * sizeMul;
        for (int k = 0; k < 4; ++k) {
            inst.color[k] = lerp(active_cfg_.life_color_start[k],
                                 active_cfg_.life_color_end[k], t);
        }
        instances_.push_back(inst);
    }
    stats_.alive = live_.size();
}

void ParticleSystem::emit_one() {
    Particle p{};
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    // Position: uniform inside disk of radius positionRadius (XY plane, Z=0).
    const float a = u01(rng_) * 6.28318530718f;
    const float r = std::sqrt(u01(rng_)) * active_cfg_.init_position_radius;
    p.pos[0] = std::cos(a) * r;
    p.pos[1] = std::sin(a) * r;
    p.pos[2] = 0.0f;

    // Velocity: angle ± spread/2, magnitude in [speedMin, speedMax].
    const float ang_deg = active_cfg_.init_velocity_angle_deg
                        + (u01(rng_) * 2.0f - 1.0f) * (active_cfg_.init_velocity_spread_deg * 0.5f);
    const float ang_rad = ang_deg * 0.01745329252f;
    const float speed   = active_cfg_.init_speed_min
                        + u01(rng_) * std::max(0.0f, active_cfg_.init_speed_max - active_cfg_.init_speed_min);
    p.vel[0] = std::cos(ang_rad) * speed;
    p.vel[1] = std::sin(ang_rad) * speed;
    p.vel[2] = 0.0f;

    // Lifetime
    const float lo = std::max(0.01f, active_cfg_.init_lifetime_min);
    const float hi = std::max(lo,    active_cfg_.init_lifetime_max);
    p.lifetime = lo + u01(rng_) * (hi - lo);
    p.age      = 0.0f;

    // Initial appearance (later modulated by sizeStart/colorStart in update()).
    p.init_size = active_cfg_.init_size;
    for (int k = 0; k < 4; ++k) p.init_color[k] = active_cfg_.init_color[k];

    live_.push_back(p);
    ++stats_.spawned;
}

} // namespace ergo::particle
