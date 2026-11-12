#include "feel_effects.h"

namespace lesson_action_feel {

const std::array<EffectInfo, kEffectCount>& effect_table() {
    static const std::array<EffectInfo, kEffectCount> table{{
        {Effect::HitStop, "hit_stop", "feel.hit_stop_last_seconds", true, "freeze the world for power / 24 s on a hit"},
        {Effect::Knockback, "knockback", "feel.knockback_last_impulse", true, "push the target by power x 6 (units / s)"},
        {Effect::CameraShake, "camera_shake", "feel.camera_shake_last_amplitude", false, "jitter the viewport by power x 0.15"},
        {Effect::HitSpark, "hit_spark", "feel.hit_spark_count", false, "sparks beside the target for 0.15 s"},
        {Effect::DamageNumbers, "damage_numbers", "feel.damage_numbers_shown", false, "the damage rising above the target"},
        {Effect::HpGauge, "hp_gauge", "feel.hp_gauge_last_drain", false, "the lost part of your HP bar drains over 0.6 s"},
        {Effect::Telegraph, "telegraph", "feel.telegraph_count", false, "the reach of a coming swing is dotted"},
        {Effect::DefeatMark, "defeat_mark", "feel.defeat_mark_count", false, "a defeated enemy stays drawn as x"},
        {Effect::BossBlur, "boss_blur", "feel.boss_blur_last_peak", false, "screen blur while the boss roars or charges"},
        {Effect::MotionBlur, "motion_blur", "feel.motion_blur_last_length", false, "trails along each object's motion vector"},
    }};
    return table;
}

const EffectInfo& effect_info(Effect e) { return effect_table()[static_cast<std::size_t>(e)]; }

std::optional<Effect> effect_by_name(std::string_view name) {
    for (const EffectInfo& i : effect_table())
        if (i.name == name) return i.id;
    return std::nullopt;
}

FeelToggles FeelToggles::all(bool on) {
    FeelToggles t;
    t.set_all(on);
    return t;
}

bool FeelToggles::all_on() const {
    for (bool b : on_)
        if (!b) return false;
    return true;
}

} // namespace lesson_action_feel
