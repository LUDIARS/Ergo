#pragma once
/// lesson_action_feel terminal — what each key does, and how a settings key
/// changes the switches (the same switches the Studio and the CLI change).
#include "key_decoder.h"

#include "combat_settings.h"
#include "feel_tuning.h"

namespace lesson_action_feel::terminal {

enum class Command {
    None,
    MoveUp, MoveDown, MoveLeft, MoveRight,
    Attack, SwitchWeapon,
    ToggleHitStop, ToggleKnockback, ToggleCameraShake,                // 1 2 3
    ToggleEnemyFlinch, CycleEnemySuperArmor, ToggleEnemyCancel,       // 4 5 6
    TogglePlayerFlinch, CyclePlayerSuperArmor, TogglePlayerCancel,    // 7 8 9
    Pause, Restart, Help, Quit,
};

Command command_for(const Key& key);

/// off -> attack -> always -> off
SuperArmor next_super_armor(SuperArmor a);

/// Apply a settings command (1..9). Returns the variable name it changed, or
/// nullptr when `c` is not a settings command.
const char* apply_setting(Command c, FeelToggles& toggles, CombatSettings& settings);

/// The key help shown under the arena (the settings keys 1-9 are shown next to their switches in the HUD).
const char* key_help();

} // namespace lesson_action_feel::terminal
