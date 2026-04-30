#pragma once

/// ergo::health — minimal HP component with damage / heal / death callbacks.
///
/// Pure data + callback. No threading, no rendering, no physics. Designed to
/// be embedded as a member of any actor / entity that needs hit points.
///
/// Spec: spec/module/health.md
/// Lexicon: spec/game-lexicon/features/core/health-system.toml (Ars)

#include <cstdint>
#include <functional>

namespace ergo::health {

/// Configuration for a single Health instance. Defaults match the
/// game-lexicon feature parameters.
struct Config {
    /// Maximum HP. Must be > 0.
    std::int32_t max_hp = 100;

    /// Auto-regen per second. 0.0 disables regen.
    float regen_per_second = 0.0f;

    /// Fire `on_death` exactly once when HP reaches 0.
    bool fire_death_event = true;
};

/// Single-instance HP container. Copyable + movable.
class Health {
public:
    using DeathHandler = std::function<void()>;
    using DamageHandler = std::function<void(std::int32_t amount, std::int32_t hp_after)>;
    using HealHandler   = std::function<void(std::int32_t amount, std::int32_t hp_after)>;

    Health() = default;
    explicit Health(Config cfg) : cfg_(cfg), hp_(cfg.max_hp) {}

    /// Apply damage. Negative amounts are clamped to 0 (use `heal()` for
    /// recovery). HP floored at 0. Fires `on_damage` then (if applicable)
    /// `on_death`.
    void apply_damage(std::int32_t amount) {
        if (amount <= 0) return;
        if (is_dead()) return;
        std::int32_t before = hp_;
        hp_ -= amount;
        if (hp_ < 0) hp_ = 0;
        if (on_damage_) on_damage_(amount, hp_);
        if (cfg_.fire_death_event && before > 0 && hp_ == 0) {
            death_fired_ = true;
            if (on_death_) on_death_();
        }
    }

    /// Heal. Capped at `max_hp`. Cannot resurrect: caller must `revive()` first.
    void heal(std::int32_t amount) {
        if (amount <= 0) return;
        if (is_dead()) return;
        hp_ += amount;
        if (hp_ > cfg_.max_hp) hp_ = cfg_.max_hp;
        if (on_heal_) on_heal_(amount, hp_);
    }

    /// Fixed-step regen tick. Caller is expected to drive this from a frame
    /// loop with the elapsed seconds. Internal float accumulator avoids
    /// dropping fractional regen on small dt.
    void tick(float dt_seconds) {
        if (is_dead()) return;
        if (cfg_.regen_per_second <= 0.0f) return;
        regen_accum_ += cfg_.regen_per_second * dt_seconds;
        if (regen_accum_ >= 1.0f) {
            std::int32_t whole = static_cast<std::int32_t>(regen_accum_);
            regen_accum_ -= static_cast<float>(whole);
            heal(whole);
        }
    }

    /// Reset HP to max and clear death state. Used for respawn.
    void revive() {
        hp_ = cfg_.max_hp;
        death_fired_ = false;
        regen_accum_ = 0.0f;
    }

    [[nodiscard]] std::int32_t hp() const noexcept { return hp_; }
    [[nodiscard]] std::int32_t max_hp() const noexcept { return cfg_.max_hp; }
    [[nodiscard]] bool is_dead() const noexcept { return hp_ == 0; }
    [[nodiscard]] float ratio() const noexcept {
        return cfg_.max_hp > 0 ? static_cast<float>(hp_) / static_cast<float>(cfg_.max_hp) : 0.0f;
    }

    void set_on_damage(DamageHandler h) { on_damage_ = std::move(h); }
    void set_on_heal(HealHandler h) { on_heal_ = std::move(h); }
    void set_on_death(DeathHandler h) { on_death_ = std::move(h); }

private:
    Config cfg_{};
    std::int32_t hp_ = 100;
    float regen_accum_ = 0.0f;
    bool death_fired_ = false;
    DamageHandler on_damage_{};
    HealHandler   on_heal_{};
    DeathHandler  on_death_{};
};

}  // namespace ergo::health
