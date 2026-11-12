#pragma once
/// lesson_action_feel — the numbers of the presentation effects (one struct,
/// like FeelTuning, so a student can change them and rebuild).
namespace lesson_action_feel {

struct MotionBlurTuning {
    float shutter      = 5.0f;   ///< trail length = this many frames of the object's own travel
    float min_length   = 0.8f;   ///< shorter trails are not drawn (slow walking shows nothing)
    float max_length   = 3.0f;   ///< clamp (world units)
    float sample_every = 0.5f;   ///< one sample per this many units along the trail
};

struct BossBlurTuning {
    float roar_peak    = 0.6f;   ///< reached at the end of the roar's active window
    float roar_windup  = 0.15f;  ///< a faint start during the roar's wind-up
    float charge_peak  = 0.7f;   ///< reached at the end of the charge (wind-up)
    float release      = 1.0f;   ///< the spike when the charge is released
    float decay        = 2.5f;   ///< 1 / s, real time (it keeps fading during the release's hit stop)
};

struct PresentationTuning {
    float shake_per_power   = 0.15f;   ///< camera shake amplitude (units) per power point
    float shake_max         = 1.0f;
    float shake_decay       = 6.0f;    ///< 1 / s, real time (the camera keeps moving during a hit stop)
    float hit_flash_seconds = 0.15f;   ///< sparks, real time
    float popup_seconds     = 0.6f;    ///< damage numbers, real time
    float gauge_hold        = 0.25f;   ///< the lost part of the HP bar stays this long ...
    float gauge_drain       = 60.0f;   ///< ... then drains at this many HP / s (real time)
    BossBlurTuning   boss_blur;
    MotionBlurTuning motion_blur;
};

} // namespace lesson_action_feel
