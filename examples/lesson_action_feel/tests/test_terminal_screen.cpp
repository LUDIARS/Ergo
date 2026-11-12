// The terminal front-end's drawing: a given view state gives the expected
// rows (arena, glyphs, the shake offset, the telegraph, the hit spark) and
// the HUD says what is switched on.
#include "terminal/ansi_frame.h"
#include "terminal/screen.h"

#include <gtest/gtest.h>

using namespace lesson_action_feel;
using namespace lesson_action_feel::terminal;

namespace {

FighterView fighter(Side side, Vec2 pos, std::int32_t hp) {
    FighterView f;
    f.side    = side;
    f.pos     = pos;
    f.facing  = side == Side::Player ? Vec2{1, 0} : Vec2{-1, 0};
    f.hp      = hp;
    f.max_hp  = 100;
    f.weapon  = side == Side::Player ? "sword" : "claw";
    f.weapon_damage = 20;
    f.weapon_power  = 2;
    f.reach   = 1.8f;
    f.arc_cos = 0.34f;
    return f;
}

/// A 6 x 3 arena: player at the left facing right, one enemy at the right.
GameView small_view() {
    GameView v;
    v.arena_width  = 6;
    v.arena_height = 3;
    v.wave         = 1;
    v.waves_to_win = 3;
    v.fighters.push_back(fighter(Side::Player, {1.2f, 1.5f}, 100));
    v.fighters.push_back(fighter(Side::Enemy, {4.2f, 1.5f}, 40));
    return v;
}

/// The arena part of the rendered screen (after the HUD rows).
std::vector<std::string> arena_rows(const std::vector<std::string>& all, std::size_t n = 7) {
    const auto first = all.begin() + static_cast<std::ptrdiff_t>(kHudRows);
    return std::vector<std::string>(first, first + static_cast<std::ptrdiff_t>(n));
}

} // namespace

TEST(LessonTerminalScreen, DrawsTheArenaFromTheView) {
    const auto rows = arena_rows(render_screen(small_view(), HudState{}));
    const std::vector<std::string> expected = {
        "                  ",
        "  ##############  ",
        "  #       40   #  ",
        "  #  @>    e   #  ",
        "  #            #  ",
        "  ##############  ",
        "                  ",
    };
    EXPECT_EQ(rows, expected);
}

TEST(LessonTerminalScreen, CameraShakeShiftsTheWholeArena) {
    GameView v      = small_view();
    v.camera_offset = {0.5f, 1.0f};   // 1 column, 1 row
    const auto rows = arena_rows(render_screen(v, HudState{}));
    EXPECT_EQ(rows[2], "   ############## ");
    EXPECT_EQ(rows[4], "   #  @>    e   # ");
}

TEST(LessonTerminalScreen, WindUpIsTelegraphedAndHitsSpark) {
    GameView v = small_view();
    v.fighters[1].action = Action::Attack;
    v.fighters[1].phase  = AttackPhase::WindUp;
    v.fighters[1].reach  = 1.4f;
    v.fighters[1].arc_cos = 0.5f;
    v.fighters[1].telegraph = true;   // the telegraph effect is on (Presentation sets this)
    auto rows = arena_rows(render_screen(v, HudState{}));
    EXPECT_EQ(rows[3], "  #  @>  ..!   #  ");   // the dots are the reach of the coming swing
    EXPECT_NE(rows[3].find('!'), std::string::npos);

    v.fighters[1].action = Action::Flinch;
    v.fighters[1].spark  = true;
    v.frozen             = true;
    const auto all = render_screen(v, HudState{});
    rows           = arena_rows(all);
    EXPECT_EQ(rows[3], "  #  @>   *~*  #  ");
    EXPECT_NE(all[1].find("HIT STOP"), std::string::npos);
}

TEST(LessonTerminalScreen, SuperArmorAndDefeatGlyphs) {
    GameView v = small_view();
    v.fighters[1].armored_now = true;
    EXPECT_EQ(arena_rows(render_screen(v, HudState{}))[3], "  #  @>    E   #  ");
    v.fighters[1].action = Action::Dead;
    const auto rows = arena_rows(render_screen(v, HudState{}));
    EXPECT_EQ(rows[3], "  #  @>    x   #  ");
    EXPECT_EQ(rows[2], "  #            #  ");   // no HP label on a corpse
}

TEST(LessonTerminalScreen, HudShowsSwitchesSidesAndProgress) {
    HudState h;
    h.toggles[Effect::Knockback]    = false;
    h.settings.enemy.super_armor    = SuperArmor::Attack;
    h.observed.enemy.flinches       = 13;
    h.observed.enemy.armor_absorbed = 3;
    h.observed.player.cancels       = 21;
    h.link    = "Studio linked";
    h.message = "feel.knockback: ON -> OFF";
    GameView v = small_view();
    v.defeated = 2;
    v.score    = 340;
    v.frozen   = true;
    const auto all = render_screen(v, h);
    const std::vector<std::string> hud(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(kHudRows));
    const std::vector<std::string> expected = {
        "HP 100/100 [##########]  SWORD dmg 20 pow 2  wave 1/3  KO 2",
        "score 340  combo 0  time 0.0s  world 0.0s  HIT STOP",
        "effects [1]hit stop ON  [2]knockback off [3]shake ON   [O]all 9/10 on [0]",
        "        flinch   super armor   motion cancel |flinched absorbed cancelled",
        "enemy   [4] ON   [5] attack    [6] off       |       13        3        0",
        "you     [7] ON   [8] off       [9] ON        |        0        0       21",
    };
    EXPECT_EQ(hud, expected);
    EXPECT_EQ(all[kHudRows + 7], "[Studio linked] feel.knockback: ON -> OFF");
    EXPECT_EQ(all.size(), kHudRows + 7u + 1u + 1u);   // HUD, arena, message, key help
    h.show_help = false;
    EXPECT_EQ(render_screen(v, h).size(), kHudRows + 7u + 1u);
}

TEST(LessonTerminalScreen, TheDefaultArenaFits80By24EvenWithBigNumbers) {
    GameView v = small_view();
    v.arena_width  = FeelTuning{}.arena_width;
    v.arena_height = FeelTuning{}.arena_height;
    v.score        = 1234567;
    v.defeated     = 9999;
    v.combo        = 999;
    v.real_seconds = v.world_seconds = 99999.9;
    HudState h;
    h.observed.enemy.flinches = 123456;
    h.message = std::string(200, 'm');
    const auto all = render_screen(v, h);
    EXPECT_EQ(all.size(), 24u);
    for (const std::string& r : all) EXPECT_LE(r.size(), kMaxColumns);
}

TEST(LessonTerminalScreen, StatusOverlays) {
    GameView v = small_view();
    v.arena_width = 30;   // room for the text
    v.status      = GameStatus::Won;
    auto all      = render_screen(v, HudState{});
    bool found    = false;
    for (const auto& r : all) found = found || r.find("YOU WIN") != std::string::npos;
    EXPECT_TRUE(found);
    v.status = GameStatus::Lost;
    all      = render_screen(v, HudState{});
    found    = false;
    for (const auto& r : all) found = found || r.find("GAME OVER") != std::string::npos;
    EXPECT_TRUE(found);
}

TEST(LessonTerminalScreen, ComposeRedrawsInPlace) {
    EXPECT_EQ(compose_frame({"ab", "c"}), "\x1b[Hab\x1b[K\r\nc\x1b[K\x1b[J");
}
