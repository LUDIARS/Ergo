#pragma once
/// lesson_action_feel — the boss's AI. Like EnemyBrain it only produces an
/// Intent, so the boss obeys the same combat rules as everyone else.
///
/// Pattern, repeated: roar, charge, charge. Between two actions it waits
/// FeelTuning::boss_cooldown (world s), walking toward the player. It roars
/// when the player is within the roar's reach and starts a charge when the
/// player is within the charge's reach (x 0.9). It never chains by motion
/// cancel (the charge is one big committed blow).
#include "combat_settings.h"
#include "combatant.h"
#include "enemy_brain.h"
#include "feel_tuning.h"

namespace lesson_action_feel {

constexpr int kBossRoar   = 0;   ///< index into Combatant::techniques
constexpr int kBossCharge = 1;

/// The technique the boss will use next (kBossRoar / kBossCharge).
int boss_next_technique(const BrainState& brain);

Intent think_boss(const Combatant& self, const Combatant& player, BrainState& brain, float dt, const FeelTuning& t);

} // namespace lesson_action_feel
