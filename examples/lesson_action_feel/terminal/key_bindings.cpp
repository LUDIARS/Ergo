#include "key_bindings.h"

#include <string>

namespace lesson_action_feel::terminal {

namespace {

/// Variable names live as long as the program (the registry's names are string literals).
const char* variable_of(Effect e) {
    static const auto names = [] {
        std::array<std::string, kEffectCount> n;
        for (const EffectInfo& i : effect_table()) n[static_cast<std::size_t>(i.id)] = "feel." + std::string(i.name);
        return n;
    }();
    return names[static_cast<std::size_t>(e)].c_str();
}

} // namespace

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
    case '0': return Command::ToggleAllEffects;
    case '4': return Command::ToggleEnemyFlinch;
    case '5': return Command::CycleEnemySuperArmor;
    case '6': return Command::ToggleEnemyCancel;
    case '7': return Command::TogglePlayerFlinch;
    case '8': return Command::CyclePlayerSuperArmor;
    case '9': return Command::TogglePlayerCancel;
    case 'o': case 'O': return Command::EffectsScreen;
    case 'v': case 'V': return Command::DebugVectors;
    case 'p': case 'P': return Command::Pause;
    case 'r': case 'R': return Command::Restart;
    case 'h': case 'H': case '?': return Command::Help;
    case 'q': case 'Q': return Command::Quit;
    default: return Command::None;
    }
}

std::optional<Effect> effect_for_screen_key(const Key& key) {
    if (key.kind != KeyKind::Char) return std::nullopt;
    const char c = key.ch >= 'A' && key.ch <= 'Z' ? static_cast<char>(key.ch - 'A' + 'a') : key.ch;
    if (c < 'a' || c >= static_cast<char>('a' + kEffectCount)) return std::nullopt;
    return static_cast<Effect>(static_cast<std::size_t>(c - 'a'));
}

char screen_key_for(Effect e) { return static_cast<char>('a' + static_cast<int>(e)); }

SuperArmor next_super_armor(SuperArmor a) {
    switch (a) {
    case SuperArmor::Off: return SuperArmor::Attack;
    case SuperArmor::Attack: return SuperArmor::Always;
    case SuperArmor::Always: return SuperArmor::Off;
    }
    return SuperArmor::Off;
}

const char* toggle_effect(Effect e, FeelToggles& toggles) {
    toggles[e] = !toggles[e];
    return variable_of(e);
}

const char* apply_setting(Command c, FeelToggles& toggles, CombatSettings& settings) {
    switch (c) {
    case Command::ToggleHitStop: return toggle_effect(Effect::HitStop, toggles);
    case Command::ToggleKnockback: return toggle_effect(Effect::Knockback, toggles);
    case Command::ToggleCameraShake: return toggle_effect(Effect::CameraShake, toggles);
    case Command::ToggleAllEffects: toggles.set_all(!toggles.all_on()); return "feel.all";
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
    return "WASD/arrows move  J/Space attack  K weapon  O effects  V vectors  P pause Q quit";
}

} // namespace lesson_action_feel::terminal
