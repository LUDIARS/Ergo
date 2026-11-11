#include "action_feel_sim.h"

#include <cmath>

namespace lesson_action_feel {

ActionFeelSim::Frame ActionFeelSim::frame(float real_dt, const FeelToggles& toggles, ergo::world_time::Engine& world) {
    Frame out;
    out.world_dt = world.update(real_dt);
    const float dt = out.world_dt;
    world_seconds_ += dt;

    // The enemy springs back to its spot; the camera shake fades. Both run on world time, so a hit stop freezes them.
    enemy_speed_ += (-tuning_.enemy_spring * enemy_offset_ - tuning_.enemy_damping * enemy_speed_) * dt;
    enemy_offset_ += enemy_speed_ * dt;
    shake_ *= std::exp(-tuning_.shake_decay * dt);

    until_attack_ -= dt;
    if (until_attack_ > 0.0f) return out;
    until_attack_ += tuning_.attack_interval;
    out.hit = true;
    ++hits_;

    if (toggles.hit_stop) {
        world.hit_stop(tuning_.hit_stop);
        if (world.is_hit_stop()) observed_.hit_stop_last_seconds = tuning_.hit_stop;   // the engine really froze
    }
    if (toggles.knockback) {
        enemy_speed_ += tuning_.knockback;
        observed_.knockback_last_impulse = std::fabs(tuning_.knockback);
    }
    if (toggles.camera_shake) shake_ = tuning_.shake;
    return out;
}

} // namespace lesson_action_feel
