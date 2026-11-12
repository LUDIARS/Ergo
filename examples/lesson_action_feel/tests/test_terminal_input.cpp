// The terminal front-end's pure input parts: bytes -> keys -> commands,
// settings keys, and the move latch (terminals send no key release).
#include "terminal/key_bindings.h"
#include "terminal/key_decoder.h"
#include "terminal/move_latch.h"

#include <gtest/gtest.h>

#include <set>
#include <string>

using namespace lesson_action_feel;
using namespace lesson_action_feel::terminal;

TEST(LessonTerminalKeys, DecodesCharsArrowsAndCtrlC) {
    KeyDecoder d;
    const auto keys = d.feed("w\x1b[A\x1b[B\x1b[C\x1b[D\x1bOA\x03 ");
    ASSERT_EQ(keys.size(), 8u);
    EXPECT_EQ(keys[0], (Key{KeyKind::Char, 'w'}));
    EXPECT_EQ(keys[1].kind, KeyKind::Up);
    EXPECT_EQ(keys[2].kind, KeyKind::Down);
    EXPECT_EQ(keys[3].kind, KeyKind::Right);
    EXPECT_EQ(keys[4].kind, KeyKind::Left);
    EXPECT_EQ(keys[5].kind, KeyKind::Up);
    EXPECT_EQ(keys[6].kind, KeyKind::CtrlC);
    EXPECT_EQ(keys[7], (Key{KeyKind::Char, ' '}));
}

TEST(LessonTerminalKeys, CompletesASequenceSplitAcrossReads) {
    KeyDecoder d;
    EXPECT_TRUE(d.feed("\x1b").empty());
    EXPECT_TRUE(d.feed("[").empty());
    const auto keys = d.feed("Cx");
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0].kind, KeyKind::Right);
    EXPECT_EQ(keys[1], (Key{KeyKind::Char, 'x'}));
}

TEST(LessonTerminalKeys, LoneEscapeIsDroppedAndUnknownSequencesSkipped) {
    KeyDecoder d;
    EXPECT_TRUE(d.feed("\x1b").empty());
    d.flush();
    const auto a = d.feed("[A");   // after a flush this is just '[' and 'A'
    ASSERT_EQ(a.size(), 2u);
    EXPECT_EQ(a[1], (Key{KeyKind::Char, 'A'}));
    const auto b = d.feed("\x1bqj\x1b[Zk");   // ESC q -> q; ESC [ Z (shift-tab) skipped
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], (Key{KeyKind::Char, 'q'}));
    EXPECT_EQ(b[1], (Key{KeyKind::Char, 'j'}));
    EXPECT_EQ(b[2], (Key{KeyKind::Char, 'k'}));
}

TEST(LessonTerminalKeys, CommandTable) {
    EXPECT_EQ(command_for({KeyKind::Char, 'w'}), Command::MoveUp);
    EXPECT_EQ(command_for({KeyKind::Char, 'A'}), Command::MoveLeft);
    EXPECT_EQ(command_for({KeyKind::Down, 0}), Command::MoveDown);
    EXPECT_EQ(command_for({KeyKind::Char, 'j'}), Command::Attack);
    EXPECT_EQ(command_for({KeyKind::Char, ' '}), Command::Attack);
    EXPECT_EQ(command_for({KeyKind::Char, 'k'}), Command::SwitchWeapon);
    EXPECT_EQ(command_for({KeyKind::Char, '1'}), Command::ToggleHitStop);
    EXPECT_EQ(command_for({KeyKind::Char, '5'}), Command::CycleEnemySuperArmor);
    EXPECT_EQ(command_for({KeyKind::Char, '9'}), Command::TogglePlayerCancel);
    EXPECT_EQ(command_for({KeyKind::Char, 'p'}), Command::Pause);
    EXPECT_EQ(command_for({KeyKind::Char, 'q'}), Command::Quit);
    EXPECT_EQ(command_for({KeyKind::CtrlC, 0}), Command::Quit);
    EXPECT_EQ(command_for({KeyKind::Char, '0'}), Command::ToggleAllEffects);
    EXPECT_EQ(command_for({KeyKind::Char, 'o'}), Command::EffectsScreen);
    EXPECT_EQ(command_for({KeyKind::Char, 'v'}), Command::DebugVectors);
    EXPECT_EQ(command_for({KeyKind::Char, 'z'}), Command::None);
}

TEST(LessonTerminalKeys, SettingsKeysChangeTheSameSwitchesAsTheStudio) {
    FeelToggles    t;
    CombatSettings s;
    EXPECT_EQ(std::string(apply_setting(Command::ToggleHitStop, t, s)), std::string("feel.hit_stop"));
    EXPECT_FALSE(t.hit_stop());
    EXPECT_EQ(std::string(apply_setting(Command::ToggleCameraShake, t, s)), std::string("feel.camera_shake"));
    EXPECT_FALSE(t.camera_shake());
    EXPECT_EQ(std::string(apply_setting(Command::CycleEnemySuperArmor, t, s)), std::string("combat.enemy.super_armor"));
    EXPECT_EQ(s.enemy.super_armor, SuperArmor::Attack);
    apply_setting(Command::CycleEnemySuperArmor, t, s);
    EXPECT_EQ(s.enemy.super_armor, SuperArmor::Always);
    apply_setting(Command::CycleEnemySuperArmor, t, s);
    EXPECT_EQ(s.enemy.super_armor, SuperArmor::Off);
    apply_setting(Command::ToggleEnemyFlinch, t, s);
    EXPECT_FALSE(s.enemy.flinch);
    apply_setting(Command::TogglePlayerCancel, t, s);
    EXPECT_FALSE(s.player.motion_cancel);
    EXPECT_TRUE(apply_setting(Command::Attack, t, s) == nullptr);
    // 0: all effects on (some were off), then all off.
    EXPECT_EQ(std::string(apply_setting(Command::ToggleAllEffects, t, s)), std::string("feel.all"));
    EXPECT_TRUE(t.all_on());
    apply_setting(Command::ToggleAllEffects, t, s);
    EXPECT_TRUE(t == FeelToggles::all(false));
    EXPECT_FALSE(s.enemy.flinch);   // the combat rules are not effects: 0 leaves them alone
}

TEST(LessonTerminalMoveLatch, ATapWalksBrieflyAndRepeatsKeepWalking) {
    MoveLatch latch(0.12f);
    EXPECT_TRUE(latch.take(1.0f / 60).move.is_zero());
    latch.press(Command::MoveRight);
    int walking = 0;
    for (int i = 0; i < 30; ++i)
        if (!latch.take(1.0f / 60).move.is_zero()) ++walking;
    EXPECT_GE(walking, 7);   // ~0.12 s at 60 Hz
    EXPECT_LE(walking, 8);

    latch.press(Command::MoveUp);
    Intent in = latch.take(1.0f / 60);
    EXPECT_TRUE(in.move == (Vec2{0, -1}));
    latch.press(Command::MoveLeft);   // a new direction replaces the old one at once
    EXPECT_TRUE(latch.take(1.0f / 60).move == (Vec2{-1, 0}));
}

TEST(LessonTerminalMoveLatch, AttackIsOneFrame) {
    MoveLatch latch;
    latch.press(Command::Attack);
    EXPECT_TRUE(latch.take(1.0f / 60).attack);
    EXPECT_FALSE(latch.take(1.0f / 60).attack);
}

TEST(LessonTerminalKeys, TheEffectsScreenReachesEveryRegistryEntry) {
    std::set<Effect> reached;
    for (char c = 'a'; c < static_cast<char>('a' + kEffectCount); ++c) {
        const auto e = effect_for_screen_key({KeyKind::Char, c});
        ASSERT_TRUE(e.has_value());
        EXPECT_EQ(screen_key_for(*e), c);
        reached.insert(*e);
    }
    EXPECT_EQ(reached.size(), kEffectCount);
    EXPECT_FALSE(effect_for_screen_key({KeyKind::Char, static_cast<char>('a' + kEffectCount)}).has_value());
    EXPECT_FALSE(effect_for_screen_key({KeyKind::Up, 0}).has_value());
    FeelToggles t;
    EXPECT_EQ(std::string(toggle_effect(Effect::MotionBlur, t)), std::string("feel.motion_blur"));
    EXPECT_FALSE(t[Effect::MotionBlur]);
}
