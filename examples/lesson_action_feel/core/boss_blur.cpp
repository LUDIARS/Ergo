#include "boss_blur.h"

#include <algorithm>
#include <cmath>

namespace lesson_action_feel {

namespace {

float phase_progress(const Combatant& c) {
    const Weapon& w   = c.weapon;
    const float   dur = c.phase == AttackPhase::WindUp ? w.windup : (c.phase == AttackPhase::Active ? w.active : w.recovery);
    if (dur <= 0.0f) return 1.0f;
    return std::clamp(1.0f - c.phase_left / dur, 0.0f, 1.0f);
}

} // namespace

ScreenBlur BossBlurEnvelope::update(const Combatant* boss, float real_dt, bool on, const BossBlurTuning& t) {
    const bool signature = boss != nullptr && boss->action == Action::Attack && (boss->weapon.roar || boss->weapon.charge);
    float      target    = 0.0f;
    level_ *= std::exp(-t.decay * real_dt);
    if (signature) {
        const float p = phase_progress(*boss);
        centre_       = boss->pos;
        if (boss->weapon.roar) {
            kind_ = BlurKind::Radial;
            if (boss->phase == AttackPhase::WindUp) target = t.roar_windup * p;
            else if (boss->phase == AttackPhase::Active) target = t.roar_windup + (t.roar_peak - t.roar_windup) * p;
        } else {
            kind_ = BlurKind::Zoom;
            if (boss->phase == AttackPhase::WindUp) target = t.charge_peak * p;
            else if (was_charging_) target = t.release;   // the release (it then fades on real time)
        }
    }
    was_charging_ = signature && boss->weapon.charge && boss->phase == AttackPhase::WindUp;

    level_ = std::max(target, level_);
    if (level_ < 0.01f) level_ = 0.0f;
    if (!on) level_ = 0.0f;

    if (signature && !in_action_) {
        in_action_   = true;
        action_peak_ = 0.0f;
    }
    if (in_action_) action_peak_ = std::max(action_peak_, level_);
    if (!signature && in_action_) {
        in_action_ = false;
        if (action_peak_ > 0.0f) last_peak_ = action_peak_;
    }

    ScreenBlur b;
    if (level_ > 0.0f) {
        b.strength = level_;
        b.kind     = kind_;
        b.centre   = centre_;
    }
    return b;
}

} // namespace lesson_action_feel
