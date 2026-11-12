#pragma once
/// lesson_action_feel — the combat rules shared by both sides: stepping a
/// fighter's action by its Intent, deciding whether a swing touches a target,
/// and resolving a hit (damage, super armor, flinch, knockback, hit-stop and
/// shake amounts). Pure functions: no clock, no I/O, no world-time calls (the
/// game applies the returned hit stop to ergo_world_time).
///
/// Precedence on a hit (spec "優先順位"):
///   1. damage always applies; HP 0 -> Dead (the action ends, nothing else matters);
///   2. super armor (if it applies at the moment of the hit) -> damage only:
///      no flinch, no interrupt, no knockback;
///   3. otherwise, flinch ON -> the action is interrupted and the fighter flinches,
///      whatever its motion-cancel setting; flinch OFF -> the action continues;
///   4. knockback (feel.knockback ON and not absorbed) pushes the fighter.
/// Hit stop and camera shake follow their feel switches on every hit, absorbed or not.
#include "combat_settings.h"
#include "combatant.h"
#include "feel_tuning.h"

namespace lesson_action_feel {

/// Super armor protects the fighter right now.
bool armor_applies(const Combatant& c, const SideSettings& s);

/// The fighter may leave its attack right now by motion cancel: in the recovery
/// of an attack that hit something, with motion_cancel ON. Never out of a flinch.
bool can_cancel(const Combatant& c, const SideSettings& s);

/// Advance one fighter by `dt` world seconds (dt > 0) following `in`.
/// Counts a motion cancel into `counters.cancels`.
void step_action(Combatant& c, const Intent& in, float dt, const SideSettings& s, const FeelTuning& t,
                 SideCounters& counters);

/// The attacker's active swing touches the target (reach + target radius, inside the arc).
bool swing_touches(const Combatant& attacker, const Combatant& target);

/// Unit direction a knockback pushes `target` hit by `attacker`, by the tuning's rule.
Vec2 knockback_direction(const Combatant& attacker, const Combatant& target, KnockbackDirection rule);

struct HitOutcome {
    std::int32_t damage           = 0;
    bool         killed           = false;
    bool         absorbed         = false;   ///< super armor took it
    bool         flinched         = false;
    bool         interrupted      = false;   ///< an attack of the target was broken (by the flinch or the death)
    float        impulse          = 0.0f;    ///< knockback impulse applied (0 = none)
    float        hit_stop_seconds = 0.0f;    ///< to request from ergo_world_time (0 = none)
    float        shake            = 0.0f;    ///< camera shake amplitude to add (0 = none)
};

/// Resolve one hit of `attacker`'s weapon on `target` (whose side has `target_side`).
HitOutcome resolve_hit(const Combatant& attacker, Combatant& target, const SideSettings& target_side,
                       const FeelToggles& toggles, const FeelTuning& t, SideCounters& target_counters);

} // namespace lesson_action_feel
