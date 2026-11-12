// The command line and the change log of the switches.
#include "options.h"
#include "settings_watch.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace lesson_action_feel;

namespace {

std::string parse(std::vector<std::string> args, Options& o) {
    args.insert(args.begin(), "ergo_example_lesson_action_feel");
    std::vector<char*> argv;
    for (std::string& a : args) argv.push_back(a.data());
    return parse_options(static_cast<int>(argv.size()), argv.data(), o);
}

} // namespace

TEST(LessonOptions, DefaultsAreTheHeadlessFullFeelGame) {
    Options o;
    EXPECT_EQ(parse({}, o), "");
    EXPECT_EQ(o.mode, Mode::Auto);
    EXPECT_EQ(o.port, 5170);
    EXPECT_TRUE(o.studio);
    const FeelToggles t = initial_toggles(o);
    EXPECT_TRUE(t.hit_stop && t.knockback && t.camera_shake);
    EXPECT_TRUE(o.combat.player.flinch);
    EXPECT_TRUE(o.combat.player.motion_cancel);
    EXPECT_EQ(o.combat.player.super_armor, SuperArmor::Off);
    EXPECT_TRUE(o.combat.enemy.flinch);
    EXPECT_FALSE(o.combat.enemy.motion_cancel);
    EXPECT_EQ(o.combat.enemy.super_armor, SuperArmor::Off);
}

TEST(LessonOptions, PlainTurnsTheThreeEffectsOff) {
    Options o;
    EXPECT_EQ(parse({"--plain", "--port", "5591"}, o), "");
    const FeelToggles t = initial_toggles(o);
    EXPECT_FALSE(t.hit_stop || t.knockback || t.camera_shake);
    EXPECT_EQ(o.port, 5591);
}

TEST(LessonOptions, EveryCombatSwitchAtStart) {
    Options o;
    EXPECT_EQ(parse({"--play", "--endless", "--no-studio", "--enemy-flinch", "off", "--enemy-super-armor", "always",
                     "--enemy-cancel", "on", "--player-flinch", "off", "--player-super-armor", "attack",
                     "--player-cancel", "off", "--knockback-dir", "backward"},
                    o),
              "");
    EXPECT_EQ(o.mode, Mode::Play);
    EXPECT_TRUE(o.endless);
    EXPECT_FALSE(o.studio);
    EXPECT_FALSE(o.combat.enemy.flinch);
    EXPECT_EQ(o.combat.enemy.super_armor, SuperArmor::Always);
    EXPECT_TRUE(o.combat.enemy.motion_cancel);
    EXPECT_FALSE(o.combat.player.flinch);
    EXPECT_EQ(o.combat.player.super_armor, SuperArmor::Attack);
    EXPECT_FALSE(o.combat.player.motion_cancel);
    EXPECT_EQ(initial_tuning(o).knockback_direction, KnockbackDirection::TargetBackward);

    Options a;
    EXPECT_EQ(parse({"--play", "--autopilot"}, a), "");
    EXPECT_TRUE(a.autopilot);

    Options s;
    EXPECT_EQ(parse({"--super-armor"}, s), "");
    EXPECT_EQ(s.combat.enemy.super_armor, SuperArmor::Attack);
}

TEST(LessonOptions, ErrorsNameTheOption) {
    Options o;
    EXPECT_EQ(parse({"--nope"}, o), "unknown option \"--nope\"");
    EXPECT_EQ(parse({"--port"}, o), "option --port needs a value");
    EXPECT_EQ(parse({"--port", "0"}, o), "bad value for --port: \"0\"");
    EXPECT_EQ(parse({"--enemy-flinch", "yes"}, o), "bad value for --enemy-flinch: \"yes\"");
    EXPECT_EQ(parse({"--player-super-armor", "2"}, o), "bad value for --player-super-armor: \"2\"");
    EXPECT_EQ(parse({"--play", "--check"}, o), "option --check conflicts with an earlier mode option");
    Options fresh;
    EXPECT_EQ(parse({"--autopilot"}, fresh), "option --autopilot needs --play");
    EXPECT_EQ(parse({"--help", "--nope"}, o), "unknown option \"--nope\"");   // an error beats --help
    Options h;
    EXPECT_EQ(parse({"-h"}, h), "");
    EXPECT_EQ(h.mode, Mode::Help);
}

TEST(LessonSettingsWatch, WordsEveryChangeOnce) {
    FeelToggles    t;
    CombatSettings s;
    SettingsWatch  w(t, s);
    EXPECT_TRUE(w.changes(t, s).empty());
    t.hit_stop           = false;
    s.enemy.super_armor  = SuperArmor::Attack;
    s.player.flinch      = false;
    s.enemy.flinch_seconds = 0.2f;
    const auto c = w.changes(t, s);
    const std::vector<std::string> expected = {
        "feel.hit_stop: ON -> OFF",
        "combat.enemy.flinch_seconds: 0.45 -> 0.20",
        "combat.enemy.super_armor: off -> attack",
        "combat.player.flinch: ON -> OFF",
    };
    EXPECT_EQ(c, expected);
    EXPECT_TRUE(w.changes(t, s).empty());
}
