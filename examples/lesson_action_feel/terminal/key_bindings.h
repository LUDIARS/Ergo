#pragma once
/// lesson_action_feel terminal — what each key does, and how a settings key
/// changes the switches (the same switches the Studio and the CLI change).
///
/// 1 2 3 toggle the course's three effects directly; O opens the effects
/// screen, where a..j toggle every entry of the effect registry; 0 turns every
/// effect on when any is off, and every effect off when all are on (the master
/// switch, like feel.all). 4-9 are the per-side combat rules (not effects).
#include "key_decoder.h"

#include "combat_settings.h"
#include "feel_effects.h"

#include <optional>

namespace lesson_action_feel::terminal {

enum class Command {
    None,
    MoveUp, MoveDown, MoveLeft, MoveRight,
    Attack, SwitchWeapon,
    ToggleHitStop, ToggleKnockback, ToggleCameraShake,                // 1 2 3
    ToggleAllEffects,                                                 // 0
    ToggleEnemyFlinch, CycleEnemySuperArmor, ToggleEnemyCancel,       // 4 5 6
    TogglePlayerFlinch, CyclePlayerSuperArmor, TogglePlayerCancel,    // 7 8 9
    EffectsScreen,                                                    // O (a..j on it toggle, so not F)
    DebugVectors,                                                     // V
    Pause, Restart, Help, Quit,
};

Command command_for(const Key& key);

/// On the effects screen: a..j -> the registry entry in table order.
std::optional<Effect> effect_for_screen_key(const Key& key);
char                  screen_key_for(Effect e);

/// off -> attack -> always -> off
SuperArmor next_super_armor(SuperArmor a);

/// Apply a settings command (0-9). Returns the variable name it changed, or
/// nullptr when `c` is not a settings command.
const char* apply_setting(Command c, FeelToggles& toggles, CombatSettings& settings);
/// Toggle one registry entry (the effects screen). Returns its variable name.
const char* toggle_effect(Effect e, FeelToggles& toggles);

/// The key help shown under the arena (1-9 and 0 are shown next to their switches in the HUD).
const char* key_help();

} // namespace lesson_action_feel::terminal
