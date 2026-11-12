#include "key_bindings.h"

namespace lesson_action_feel::terminal {

Command command_for(const Key& key) {
    switch (key.kind) {
    case KeyKind::Up: return Command::MoveUp;
    case KeyKind::Down: return Command::MoveDown;
    case KeyKind::Left: return Command::MoveLeft;
    case KeyKind::Right: return Command::MoveRight;
    case KeyKind::CtrlC: return Command::Quit;
    case KeyKind::Char: break;
    }
    switch (key.ch) {
    case 'w': case 'W': return Command::MoveUp;
    case 's': case 'S': return Command::MoveDown;
    case 'a': case 'A': return Command::MoveLeft;
    case 'd': case 'D': return Command::MoveRight;
    case 'j': case 'J': case ' ': return Command::Attack;
    case 'k': case 'K': case '\t': return Command::SwitchWeapon;
    case '1': return Command::ToggleHitStop;
    case '2': return Command::ToggleKnockback;
    case '3': return Command::ToggleCameraShake;
    case '4': return Command::ToggleEnemyFlinch;
    case '5': return Command::CycleEnemySuperArmor;
    case '6': return Command::ToggleEnemyCancel;
    case '7': return Command::TogglePlayerFlinch;
    case '8': return Command::CyclePlayerSuperArmor;
    case '9': return Command::TogglePlayerCancel;
    case 'p': case 'P': return Command::Pause;
    case 'r': case 'R': return Command::Restart;
    case 'h': case 'H': case '?': return Command::Help;
    case 'q': case 'Q': return Command::Quit;
    default: return Command::None;
    }
}

SuperArmor next_super_armor(SuperArmor a) {
    switch (a) {
    case SuperArmor::Off: return SuperArmor::Attack;
    case SuperArmor::Attack: return SuperArmor::Always;
    case SuperArmor::Always: return SuperArmor::Off;
    }
    return SuperArmor::Off;
}

const char* apply_setting(Command c, FeelToggles& toggles, CombatSettings& settings) {
    switch (c) {
    case Command::ToggleHitStop: toggles.hit_stop = !toggles.hit_stop; return "feel.hit_stop";
    case Command::ToggleKnockback: toggles.knockback = !toggles.knockback; return "feel.knockback";
    case Command::ToggleCameraShake: toggles.camera_shake = !toggles.camera_shake; return "feel.camera_shake";
    case Command::ToggleEnemyFlinch: settings.enemy.flinch = !settings.enemy.flinch; return "combat.enemy.flinch";
    case Command::CycleEnemySuperArmor:
        settings.enemy.super_armor = next_super_armor(settings.enemy.super_armor);
        return "combat.enemy.super_armor";
    case Command::ToggleEnemyCancel:
        settings.enemy.motion_cancel = !settings.enemy.motion_cancel;
        return "combat.enemy.motion_cancel";
    case Command::TogglePlayerFlinch: settings.player.flinch = !settings.player.flinch; return "combat.player.flinch";
    case Command::CyclePlayerSuperArmor:
        settings.player.super_armor = next_super_armor(settings.player.super_armor);
        return "combat.player.super_armor";
    case Command::TogglePlayerCancel:
        settings.player.motion_cancel = !settings.player.motion_cancel;
        return "combat.player.motion_cancel";
    default: return nullptr;
    }
}

const char* key_help() {
    return "move WASD/arrows  attack J/Space  weapon K  pause P  restart R  help H  quit Q";
}

} // namespace lesson_action_feel::terminal
