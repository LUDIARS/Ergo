#pragma once
/// lesson_action_feel — the boss blur envelope (a full-screen effect, not per
/// object): it follows the boss's signature actions.
///
///   roar:   wind-up  -> faint, rising to roar_windup;
///           active   -> rising on from roar_windup to roar_peak over the active window (radial, centred on the boss);
///   charge: wind-up  -> rising from 0 to charge_peak over the whole charge (zoom, centred on the boss);
///           release  -> a spike to `release` the moment the charge becomes active;
///   after either, and whenever the target falls, the level fades by e^-decay per real second.
/// The phase progress is world time (it is the boss's action); the fade is real
/// time, so the blur keeps animating through the hit stop the release causes.
/// Deterministic; 0 when there is no boss action or the switch is off.
#include "combatant.h"
#include "game_view.h"
#include "presentation_tuning.h"

namespace lesson_action_feel {

class BossBlurEnvelope {
public:
    /// `boss` may be null. Returns the blur to draw this frame.
    ScreenBlur update(const Combatant* boss, float real_dt, bool on, const BossBlurTuning& t);

    /// Peak of the latest boss action that ended with a blur (0 until one).
    double last_peak() const { return last_peak_; }

private:
    float    level_        = 0.0f;
    BlurKind kind_         = BlurKind::None;
    Vec2     centre_;
    bool     was_charging_ = false;
    bool     in_action_    = false;
    float    action_peak_  = 0.0f;
    double   last_peak_    = 0.0;
};

} // namespace lesson_action_feel
