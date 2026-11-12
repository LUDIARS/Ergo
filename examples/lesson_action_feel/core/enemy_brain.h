#pragma once
/// lesson_action_feel — the enemies' simple AI. It produces the same Intent a
/// keyboard produces for the player, so enemies obey the same combat rules.
///
/// Walk toward the player until it is within reach, then (when its cooldown
/// is over and an attack slot is free) attack facing the player: the wind-up
/// is the visible telegraph. With motion cancel ON for enemies, an enemy whose
/// attack hit chains up to FeelTuning::enemy_chain_max follow-ups by cancelling
/// the recovery while the player is still in reach.
#include "combat_settings.h"
#include "combatant.h"
#include "feel_tuning.h"

namespace lesson_action_feel {

struct BrainState {
    float cooldown = 0.0f;   ///< world s until it may start a fresh attack
    int   chain    = 0;      ///< follow-ups chained in the current string
};

/// `may_attack`: an attack slot is free (FeelTuning::enemy_max_attackers).
Intent think(const Combatant& self, const Combatant& player, BrainState& brain, const SideSettings& side,
             bool may_attack, float dt, const FeelTuning& t);

} // namespace lesson_action_feel
