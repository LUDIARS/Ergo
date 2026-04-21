#include "ergo/world_time/world_time.h"

#include <algorithm>
#include <vector>

namespace ergo::world_time {

struct Engine::Impl {
    Phase phase = Phase::None;
    float time_scale = 1.0f;

    // HitStop
    float hit_stop_remaining = 0.0f;

    // HitSlow
    float slow_duration       = 0.0f;
    float slow_center_scale   = 1.0f;
    float slow_hold_time      = 0.0f;
    Ease  slow_ease           = Ease::Linear;
    float slow_elapsed        = 0.0f;
    bool  is_hit_slow         = false;

    std::vector<ITimeScaleTarget*> targets;

    void reset_to_idle() {
        phase = Phase::None;
        time_scale = 1.0f;
        hit_stop_remaining = 0.0f;
        is_hit_slow = false;
        slow_elapsed = 0.0f;
    }

    float advance(float real_dt) {
        if (real_dt < 0.0f) real_dt = 0.0f;

        if (!is_hit_slow && hit_stop_remaining <= 0.0f) {
            // No active effect.
            phase = Phase::None;
            time_scale = 1.0f;
            return real_dt * time_scale;
        }

        if (is_hit_slow) {
            slow_elapsed += real_dt;
            float scale = compute_slow_scale();
            // Phase classification was set inside compute_slow_scale.
            time_scale = scale;
            // HitSlow uses elapsed, not remaining. End when elapsed >= duration.
            if (slow_elapsed >= slow_duration) {
                reset_to_idle();
                time_scale = 1.0f;
            }
        } else {
            // HitStopActive
            time_scale = 0.0f;
            phase = Phase::HitStopActive;
            hit_stop_remaining -= real_dt;
            if (hit_stop_remaining <= 0.0f) {
                reset_to_idle();
            }
        }
        return real_dt * time_scale;
    }

    float compute_slow_scale() {
        // Curve: In (transition) -> Loop (hold) -> Out (transition)
        // transition = (duration - hold) / 2, both >= 0.
        float total      = slow_duration;
        float hold       = slow_hold_time;
        if (hold > total) hold = total;
        float transition = (total - hold) * 0.5f;

        if (transition <= 0.0f) {
            // Pure hold (no In/Out lerp)
            phase = Phase::HitSlowLoop;
            return slow_center_scale;
        }

        if (slow_elapsed < transition) {
            phase = Phase::HitSlowIn;
            float t = slow_elapsed / transition;
            float k = ease(t, slow_ease);
            return lerp(1.0f, slow_center_scale, k);
        }
        if (slow_elapsed < transition + hold) {
            phase = Phase::HitSlowLoop;
            return slow_center_scale;
        }
        phase = Phase::HitSlowOut;
        float out_progress = (slow_elapsed - transition - hold) / transition;
        float k = ease(out_progress, slow_ease);
        return lerp(slow_center_scale, 1.0f, k);
    }

    void notify(float scale) {
        // Walk in reverse so we can prune nullptrs in place.
        for (std::size_t i = targets.size(); i > 0; ) {
            --i;
            auto* t = targets[i];
            if (t == nullptr) {
                targets.erase(targets.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            t->on_time_scale_update(scale);
        }
    }
};

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine()  : impl_(new Impl()) {}
Engine::~Engine() { delete impl_; }

void Engine::hit_stop(float duration_seconds) {
    if (!(duration_seconds > 0.0f)) return;
    impl_->reset_to_idle();
    impl_->hit_stop_remaining = duration_seconds;
    impl_->is_hit_slow = false;
    impl_->phase = Phase::HitStopActive;
    impl_->time_scale = 0.0f;
}

void Engine::hit_slow(float duration_seconds,
                      float /*center_weight*/,
                      float center_time_scale,
                      float center_hold_time,
                      Ease  e) {
    if (!(duration_seconds > 0.0f)) return;
    impl_->reset_to_idle();
    impl_->is_hit_slow       = true;
    impl_->slow_duration     = duration_seconds;
    impl_->slow_center_scale = center_time_scale;
    impl_->slow_hold_time    = center_hold_time < 0.0f ? 0.0f : center_hold_time;
    impl_->slow_ease         = e;
    impl_->slow_elapsed      = 0.0f;
    impl_->phase             = Phase::HitSlowIn;
    impl_->time_scale        = 1.0f;   // computed properly on first update()
}

void Engine::force_stop() {
    impl_->reset_to_idle();
    impl_->notify(1.0f);
}

float Engine::update(float real_dt) {
    float effective = impl_->advance(real_dt);
    impl_->notify(impl_->time_scale);
    return effective;
}

void Engine::register_target(ITimeScaleTarget* target) {
    if (target == nullptr) return;
    auto& v = impl_->targets;
    if (std::find(v.begin(), v.end(), target) != v.end()) return;
    v.push_back(target);
}

void Engine::unregister_target(ITimeScaleTarget* target) {
    if (target == nullptr) return;
    auto& v = impl_->targets;
    v.erase(std::remove(v.begin(), v.end(), target), v.end());
}

Phase Engine::current_phase()      const { return impl_->phase; }
float Engine::current_time_scale() const { return impl_->time_scale; }

bool Engine::is_hit_stop() const {
    return impl_->phase == Phase::HitStopActive;
}

bool Engine::is_hit_slow() const {
    switch (impl_->phase) {
    case Phase::HitSlowIn:
    case Phase::HitSlowLoop:
    case Phase::HitSlowOut:
        return true;
    default:
        return false;
    }
}

std::size_t Engine::target_count() const { return impl_->targets.size(); }

} // namespace ergo::world_time
