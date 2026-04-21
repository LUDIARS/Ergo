#pragma once

/// ergo::world_time — global time-scale composer for hit-stop / hit-slow
/// gameplay effects.
///
/// Port of the WorldTimeScale feature from VGA-Team2026/Foundation (Unity)
/// to plain C++17. Hosts call `update(real_dt)` once per frame to advance
/// the composer and obtain the simulation `dt`. Gameplay code uses that dt;
/// observers (camera, particle systems, audio) can additionally subscribe
/// via `ITimeScaleTarget` to react to the current scale directly.
///
/// Single-active-effect model: a fresh `hit_stop` / `hit_slow` overrides
/// any in-progress effect. Use `force_stop` to cancel without starting a
/// new one.

#include "ergo/world_time/easing.h"

#include <cstddef>

namespace ergo::world_time {

/// Phases of the composer state machine.
enum class Phase {
    None,            ///< no active effect, time_scale = 1.0
    HitStopActive,   ///< full freeze (time_scale = 0)
    HitSlowIn,       ///< 1.0 -> center_time_scale (eased)
    HitSlowLoop,     ///< holding at center_time_scale
    HitSlowOut,      ///< center_time_scale -> 1.0 (eased)
};

/// Subscriber that wants to receive per-frame time-scale notifications.
///
/// `Engine::update` calls `on_time_scale_update(scale)` for every
/// registered target after computing the new scale. Targets must remain
/// alive while registered; null entries are detected and pruned, but
/// dangling pointers are undefined behavior — call `unregister_target`
/// in your destructor.
class ITimeScaleTarget {
public:
    virtual ~ITimeScaleTarget() = default;
    virtual void on_time_scale_update(float time_scale) = 0;
};

/// Marker alias to mirror the Foundation naming convention. Functionally
/// identical to `ITimeScaleTarget`.
using IHitStopTarget = ITimeScaleTarget;

class Engine {
public:
    static Engine& instance();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // ---- Effect triggers (single-active, overwrite) ----------------------

    /// Freeze the world for `duration_seconds`. Replaces any in-progress
    /// effect. `duration_seconds <= 0` is a no-op.
    void hit_stop(float duration_seconds);

    /// Three-phase slow motion: lerp 1.0 -> `center_time_scale` over the
    /// transition window, hold for `center_hold_time`, then lerp back.
    /// Replaces any in-progress effect.
    ///
    /// Parameters:
    /// - `duration_seconds`     total effect length, must be > 0
    /// - `center_weight`        0..1; reserved for future weighting (kept
    ///                          for Foundation API parity, currently unused
    ///                          inside the curve)
    /// - `center_time_scale`    scale during the Loop phase (typically
    ///                          0.0..1.0 but unclamped — feel free to use
    ///                          values >1 for fast-forward)
    /// - `center_hold_time`     hold duration; clamped to <= duration
    /// - `e`                    easing applied to the In and Out lerps
    void hit_slow(float duration_seconds,
                  float center_weight,
                  float center_time_scale,
                  float center_hold_time,
                  Ease  e = Ease::InOutQuad);

    /// Cancel any active effect immediately and notify observers with
    /// scale = 1.0 once.
    void force_stop();

    // ---- Per-frame -------------------------------------------------------

    /// Advance the composer by `real_dt` seconds (negative is treated as
    /// 0). Returns the simulation dt (`real_dt * current_time_scale`).
    /// Also notifies every registered observer.
    float update(float real_dt);

    // ---- Observers -------------------------------------------------------

    /// Register a target. Duplicate registrations are ignored.
    void register_target(ITimeScaleTarget* target);

    /// Remove a previously-registered target. Idempotent.
    void unregister_target(ITimeScaleTarget* target);

    // ---- Queries ---------------------------------------------------------

    Phase       current_phase()     const;
    float       current_time_scale() const;
    bool        is_hit_stop()       const;
    bool        is_hit_slow()       const;
    std::size_t target_count()      const;

private:
    Engine();
    ~Engine();
    struct Impl;
    Impl* impl_;
};

} // namespace ergo::world_time
