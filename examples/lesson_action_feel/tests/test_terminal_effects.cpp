// How the terminal draws the effects a terminal cannot really do: the boss
// blur (speed lines, weak vs strong, radial vs zoom), the motion-blur trail
// (aspect-corrected, fading), the debug motion vectors, damage numbers, the
// HP gauge and the effects screen.
#include "terminal/screen.h"
#include "terminal/key_bindings.h"
#include "terminal/screen_effects.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

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

GameView small_view() {
    GameView v;
    v.arena_width  = 6;
    v.arena_height = 3;
    v.wave         = 1;
    v.waves_to_win = 4;
    v.fighters.push_back(fighter(Side::Player, {1.2f, 1.5f}, 100));
    v.fighters.push_back(fighter(Side::Enemy, {4.2f, 1.5f}, 40));
    return v;
}

/// A 12 x 7 arena with the boss in the middle.
GameView boss_view() {
    GameView v;
    v.arena_width  = 12;
    v.arena_height = 7;
    v.wave         = 4;
    v.waves_to_win = 4;
    v.boss_wave    = true;
    v.fighters.push_back(fighter(Side::Player, {2.2f, 3.5f}, 100));
    FighterView boss = fighter(Side::Enemy, {6.2f, 3.5f}, 400);
    boss.boss   = true;
    boss.radius = 1.0f;
    v.fighters.push_back(boss);
    return v;
}

std::vector<std::string> arena(const GameView& v, const HudState& h = {}) {
    const auto all   = render_screen(v, h);
    const auto first = all.begin() + static_cast<std::ptrdiff_t>(kHudRows);
    return std::vector<std::string>(first, first + static_cast<std::ptrdiff_t>(v.arena_height) + 4);
}

} // namespace

TEST(LessonTerminalEffects, TrailCellsFollowTheStreakWithTheCellAspect) {
    // 1 world unit backwards along x = 2 columns; the ramp fades away from the body.
    EXPECT_EQ(trail_cells({-1.0f, 0.0f}), (std::vector<GlyphCell>{{-1, 0, ';'}, {-2, 0, ','}}));
    // 2.5 units down = 3 rows (rounded), near to far: % ; ,
    EXPECT_EQ(trail_cells({0.0f, 2.5f}), (std::vector<GlyphCell>{{0, 1, '%'}, {0, 2, ';'}, {0, 3, ','}}));
    // A diagonal stays diagonal on screen: (-0.6, -0.8) units = (-1.2 cols, -0.8 rows).
    EXPECT_EQ(trail_cells({-0.6f, -0.8f}), (std::vector<GlyphCell>{{-1, -1, ','}}));
    EXPECT_TRUE(trail_cells({0.1f, 0.0f}).empty());   // shorter than half a cell: nothing
}

TEST(LessonTerminalEffects, DebugVectorsPointForwardWithAHead) {
    EXPECT_EQ(vector_cells({0.15f, 0.0f}), (std::vector<GlyphCell>{{1, 0, '-'}, {2, 0, '-'}, {3, 0, '>'}}));
    EXPECT_EQ(vector_cells({0.0f, -0.2f}), (std::vector<GlyphCell>{{0, -1, '|'}, {0, -2, '^'}}));
    EXPECT_EQ(vector_cells({0.1f, 0.1f}), (std::vector<GlyphCell>{{1, 1, '\\'}, {2, 1, '\\'}}));
    EXPECT_TRUE(vector_cells({}).empty());
}

TEST(LessonTerminalEffects, ATrailIsDrawnBehindTheMovingEnemyOnly) {
    GameView v          = small_view();
    v.fighters[1].streak = {1.2f, 0.0f};   // moving left: the trail is on its right
    EXPECT_EQ(arena(v)[3], "  #  @>    e;, #  ");
    v.fighters[1].streak = {};
    EXPECT_EQ(arena(v)[3], "  #  @>    e   #  ");
}

TEST(LessonTerminalEffects, BossBlurIsSpeedLinesWeakOrStrong) {
    GameView none = boss_view();
    const std::vector<std::string> idle = {
        "                              ",
        "  ##########################  ",
        "  #                        #  ",
        "  #                        #  ",
        "  #           400          #  ",
        "  #    @>      B           #  ",
        "  #                        #  ",
        "  #                        #  ",
        "  #                        #  ",
        "  ##########################  ",
        "                              ",
    };
    EXPECT_EQ(arena(none), idle);

    GameView weak = boss_view();
    weak.blur     = {0.3f, BlurKind::Radial, {6.2f, 3.5f}};
    const std::vector<std::string> radial_weak = {
        "                              ",
        "  ##########################  ",
        "  #      \\     |     /     #  ",
        "  #        \\   |   /       #  ",
        "  #           400          #  ",
        "  # ---@>----  B  ---------#  ",
        "  #                        #  ",
        "  #        /   |   \\       #  ",
        "  #      /     |     \\     #  ",
        "  ##########################  ",
        "                              ",
    };
    EXPECT_EQ(arena(weak), radial_weak);

    GameView strong = boss_view();
    strong.blur     = {0.9f, BlurKind::Radial, {6.2f, 3.5f}};
    const std::vector<std::string> radial_strong = {
        "                              ",
        "  ##########################  ",
        "  #      \\  || | ||  /     #  ",
        "  # ----   \\ ||||| /   ----#  ",
        "  #      ---  400  ---     #  ",
        "  #----@>----  B  ---------#  ",
        "  #      ---       ---     #  ",
        "  # ----   / ||||| \\   ----#  ",
        "  #      /  || | ||  \\     #  ",
        "  ##########################  ",
        "                              ",
    };
    EXPECT_EQ(arena(strong), radial_strong);

    GameView zoom = boss_view();
    zoom.blur     = {0.7f, BlurKind::Zoom, {6.2f, 3.5f}};
    const std::vector<std::string> converging = {
        "                              ",
        "  ##########################  ",
        "  #                        #  ",
        "  # ---                 ---#  ",
        "  #           400          #  ",
        "  #--- @>      B         --#  ",
        "  #                        #  ",
        "  # ---                 ---#  ",
        "  #                        #  ",
        "  ##########################  ",
        "                              ",
    };
    EXPECT_EQ(arena(zoom), converging);
    // The HUD stays sharp and says how strong it is.
    EXPECT_EQ(render_screen(strong, HudState{})[1], "score 0  combo 0  time 0.0s  world 0.0s  blur 0.90");
}

TEST(LessonTerminalEffects, BossBlurGlyphMapping) {
    EXPECT_EQ(boss_blur_glyph(10, 0, 0.04f, BlurKind::Radial), 0);   // too weak
    EXPECT_EQ(boss_blur_glyph(1, 0, 0.9f, BlurKind::Radial), 0);     // the boss itself stays clear
    EXPECT_EQ(boss_blur_glyph(6, 0, 0.5f, BlurKind::Radial), '-');
    EXPECT_EQ(boss_blur_glyph(0, 3, 0.5f, BlurKind::Radial), '|');
    EXPECT_EQ(boss_blur_glyph(30, 0, 0.3f, BlurKind::Radial), 0);    // beyond the radial reach at 0.3
    EXPECT_EQ(boss_blur_glyph(30, 0, 0.3f, BlurKind::Zoom), '-');    // the zoom comes from the outside
    EXPECT_EQ(boss_blur_glyph(6, 0, 0.3f, BlurKind::Zoom), 0);
    EXPECT_EQ(boss_blur_glyph(6, 0, 0.9f, BlurKind::None), 0);
}

TEST(LessonTerminalEffects, DebugVectorsAndTheirLine) {
    GameView v              = small_view();
    v.fighters[0].motion    = {0.15f, 0.0f};
    v.fighters[1].id        = 7;
    v.fighters[1].motion    = {0.2f, 0.0f};
    HudState h;
    h.debug_vectors = true;
    h.last_hit_id   = 7;
    const auto all = render_screen(v, h);
    EXPECT_EQ(all[kHudRows + 3], "  #  @>->  e---#  ");   // clipped at the wall, bodies kept
    EXPECT_EQ(all.back(), "motion u/frame: you(+0.15,+0.00)  hit#7(+0.20,+0.00)  cam(+0.00,+0.00)");
}

TEST(LessonTerminalEffects, DamageNumbersAndTheHpGauge) {
    GameView v = small_view();
    v.popups.push_back({{4.2f, 1.5f}, 20, 0.1f});
    EXPECT_EQ(arena(v)[2], "  #       40 -2#  ");   // a number never covers the wall
    GameView g             = small_view();
    g.fighters[0].hp       = 60;
    g.player_hp_trail      = 88;
    EXPECT_EQ(render_screen(g, HudState{})[0].substr(0, 23), "HP  60/100 [######===.]");
}

TEST(LessonTerminalEffects, TheEffectsScreenListsEveryEffect) {
    GameView v     = boss_view();
    v.arena_width  = 30;
    v.arena_height = 12;
    HudState h;
    h.effects_screen         = true;
    h.toggles[Effect::BossBlur] = false;
    const auto rows = arena(v, h);
    EXPECT_EQ(rows[2], "  #  EFFECTS (paused)  a-j toggle  0 all off/on  O close       #  ");
    for (const EffectInfo& e : effect_table()) {
        bool listed = false;
        for (const auto& r : rows)
            listed = listed || (r.find(std::string(1, screen_key_for(e.id)) + "  " + std::string(e.name)) != std::string::npos &&
                                r.find("feel." + std::string(e.name)) != std::string::npos);
        EXPECT_TRUE(listed);
    }
    EXPECT_NE(rows[11].find("boss_blur      off  visual"), std::string::npos);
    EXPECT_NE(rows[3].find("hit_stop       ON   gameplay"), std::string::npos);
}
