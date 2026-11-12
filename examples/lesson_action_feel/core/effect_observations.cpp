#include "effect_observations.h"

namespace lesson_action_feel {

double effect_observation(Effect e, const GameSession& s) {
    const FeelObserved&         g = s.game().observed();
    const PresentationObserved& p = s.fx().observed();
    switch (e) {
    case Effect::HitStop: return g.hit_stop_last_seconds;
    case Effect::Knockback: return g.knockback_last_impulse;
    case Effect::CameraShake: return p.camera_shake_last_amplitude;
    case Effect::HitSpark: return static_cast<double>(p.hit_spark_count);
    case Effect::DamageNumbers: return static_cast<double>(p.damage_numbers_shown);
    case Effect::HpGauge: return p.hp_gauge_last_drain;
    case Effect::Telegraph: return static_cast<double>(p.telegraph_count);
    case Effect::DefeatMark: return static_cast<double>(p.defeat_mark_count);
    case Effect::BossBlur: return p.boss_blur_last_peak;
    case Effect::MotionBlur: return p.motion_blur_last_length;
    case Effect::Count: break;
    }
    return 0.0;
}

} // namespace lesson_action_feel
