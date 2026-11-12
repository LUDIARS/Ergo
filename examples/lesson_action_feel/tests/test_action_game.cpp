// The arena game as a whole: the feel effects through the real
// ergo_world_time engine, waves and defeat, win and loss, and one scripted
// scenario per combat concept showing the observable difference.
#include "action_game.h"
#include "autopilot.h"

#include <gtest/gtest.h>

#include <functional>

using namespace lesson_action_feel;

namespace {

constexpr float kDt = 1.0f / 60.0f;

ergo::world_time::Engine& fresh_world() {
    auto& w = ergo::world_time::Engine::instance();
    w.force_stop();
    return w;
}

using Script = std::function<PlayerCommand(const GameView&)>;

PlayerCommand stand(const GameView&) { return {}; }

/// Run `seconds` of real time; returns the number of frames whose world dt was 0.
int run(ActionGame& g, float seconds, const Script& script, const FeelToggles& t, const CombatSettings& s,
        ergo::world_time::Engine& w) {
    int frozen = 0;
    const int frames = static_cast<int>(seconds * 60.0f + 0.5f);
    for (int i = 0; i < frames; ++i)
        if (g.frame(kDt, script(g.view(s)), t, s, w).world_dt == 0.0f) ++frozen;
    return frozen;
}

/// Face the nearest live enemy without walking; attack only while it is winding up and in reach.
PlayerCommand punish_windups(const GameView& v) {
    PlayerCommand cmd;
    const FighterView& me = v.fighters.front();
    for (std::size_t i = 1; i < v.fighters.size(); ++i) {
        const FighterView& e = v.fighters[i];
        if (e.action == Action::Dead) continue;
        const Vec2 to = e.pos - me.pos;
        cmd.intent.aim = to;
        if (e.action == Action::Attack && e.phase == AttackPhase::WindUp && to.length() <= me.reach + e.radius)
            cmd.intent.attack = true;
        return cmd;
    }
    return cmd;
}

/// Face the nearest live enemy without walking and attack whenever it is in reach.
PlayerCommand stand_and_swing(const GameView& v) {
    PlayerCommand cmd;
    const FighterView& me = v.fighters.front();
    for (std::size_t i = 1; i < v.fighters.size(); ++i) {
        const FighterView& e = v.fighters[i];
        if (e.action == Action::Dead) continue;
        const Vec2 to = e.pos - me.pos;
        cmd.intent.aim    = to;
        cmd.intent.attack = to.length() <= me.reach + e.radius;
        return cmd;
    }
    return cmd;
}

const FeelToggles kOff = FeelToggles::all(false);

FeelToggles only(Effect e) {
    FeelToggles t = FeelToggles::all(false);
    t[e]          = true;
    return t;
}

} // namespace

TEST(LessonActionGame, StartsWithThePlayerAndTheFirstWave) {
    ActionGame g;
    EXPECT_EQ(g.wave(), 1);
    EXPECT_EQ(g.enemy_count(), 2u);
    EXPECT_EQ(g.player().health.hp(), 100);
    EXPECT_EQ(g.status(), GameStatus::Playing);
    const GameView v = g.view(CombatSettings{});
    ASSERT_EQ(v.fighters.size(), 3u);
    EXPECT_EQ(v.fighters[0].side, Side::Player);
    EXPECT_EQ(v.waves_to_win, 4);   // three waves, then the boss
}

TEST(LessonActionGame, EffectsOffObservationsStayZeroAndTheWorldNeverFreezes) {
    auto&            w = fresh_world();
    ActionGame       g;
    const CombatSettings s;
    const int frozen = run(g, 6.0f, autopilot, kOff, s, w);
    EXPECT_GE(g.player_hits(), 5);
    EXPECT_EQ(frozen, 0);
    EXPECT_TRUE(g.observed().hit_stop_last_seconds == 0.0);
    EXPECT_TRUE(g.observed().knockback_last_impulse == 0.0);
    EXPECT_TRUE(g.knockback_travel() == 0.0);
    EXPECT_DOUBLE_EQ(g.world_seconds(), g.real_seconds());
}

TEST(LessonActionGame, HitStopFreezesTheWorldForPowerOver24Seconds) {
    auto&          w = fresh_world();
    ActionGame     g;
    CombatSettings s;
    const FeelToggles t = only(Effect::HitStop);
    // Walk to the first enemy until the first hit lands.
    int guard = 0;
    while (g.player_hits() == 0 && guard++ < 600) g.frame(kDt, autopilot(g.view(s)), t, s, w);
    ASSERT_EQ(g.player_hits(), 1);
    EXPECT_TRUE(w.is_hit_stop());
    EXPECT_NEAR(g.observed().hit_stop_last_seconds, 2.0 / 24.0, 1e-6);   // the sword's power is 2
    // The world stands still ~5 frames (0.083 s at 60 Hz); real time and the HUD clock keep going.
    const double world_before = g.world_seconds();
    const double real_before  = g.real_seconds();
    int frozen = 0;
    while (g.frame(kDt, PlayerCommand{}, t, s, w).world_dt == 0.0f && frozen < 60) ++frozen;
    EXPECT_GE(frozen, 4);
    EXPECT_LE(frozen, 5);
    EXPECT_LT(g.world_seconds() - world_before, g.real_seconds() - real_before);
}

TEST(LessonActionGame, TimersRunOnWorldTimeSoAHitStopHoldsAFlinch) {
    auto&          w = fresh_world();
    ActionGame     g;
    CombatSettings s;
    const FeelToggles t = only(Effect::HitStop);
    int guard = 0;
    while (g.player_hits() == 0 && guard++ < 600) g.frame(kDt, autopilot(g.view(s)), t, s, w);
    ASSERT_TRUE(w.is_hit_stop());
    const Combatant* hit = nullptr;
    for (std::size_t i = 0; i < g.enemy_count(); ++i)
        if (g.enemy(i).action == Action::Flinch) hit = &g.enemy(i);
    ASSERT_NE(hit, nullptr);
    const float left = hit->flinch_left;
    g.frame(kDt, PlayerCommand{}, t, s, w);   // frozen frame
    EXPECT_FLOAT_EQ(hit->flinch_left, left);
}

TEST(LessonActionGame, KnockbackPushesTheEnemyAwayAndIsObserved) {
    auto&          w = fresh_world();
    ActionGame     g;
    CombatSettings s;
    const FeelToggles t = only(Effect::Knockback);
    int guard = 0;
    while (g.player_hits() == 0 && guard++ < 600) g.frame(kDt, autopilot(g.view(s)), t, s, w);
    ASSERT_EQ(g.player_hits(), 1);
    EXPECT_DOUBLE_EQ(g.observed().knockback_last_impulse, 12.0);   // power 2 x 6
    run(g, 0.5f, stand, t, s, w);
    EXPECT_GT(g.knockback_travel(), 1.0);   // 12 / friction 10 = 1.2 units
}

TEST(LessonActionGame, DefeatedEnemiesLeaveAndTheNextWaveComes) {
    auto&          w = fresh_world();
    ActionGame     g;
    CombatSettings s;
    int guard = 0;
    while (g.wave() == 1 && guard++ < 60 * 60) g.frame(kDt, autopilot(g.view(s)), FeelToggles{}, s, w);
    EXPECT_EQ(g.wave(), 2);
    EXPECT_EQ(g.defeated(), 2);
    EXPECT_EQ(g.enemy_count(), 3u);
    EXPECT_GE(g.score(), 2 * 100);
}

TEST(LessonActionGame, TheScriptedPlayerClearsAllWavesAndTheBossAndWins) {
    auto&          w = fresh_world();
    ActionGame     g;
    CombatSettings s;
    int frames = 0;
    while (g.status() == GameStatus::Playing && frames++ < 60 * 180) g.frame(kDt, autopilot(g.view(s)), FeelToggles{}, s, w);
    EXPECT_EQ(g.status(), GameStatus::Won);
    EXPECT_EQ(g.defeated(), 2 + 3 + 4 + 1);   // the waves and the boss
    EXPECT_EQ(g.wave(), 4);
    EXPECT_GT(g.player().health.hp(), 0);
    // After the win nothing happens any more.
    const auto hits = g.player_hits();
    run(g, 1.0f, autopilot, FeelToggles{}, s, w);
    EXPECT_EQ(g.player_hits(), hits);
}

TEST(LessonActionGame, StandingStillLoses) {
    auto&          w = fresh_world();
    ActionGame     g;
    CombatSettings s;
    int frames = 0;
    while (g.status() == GameStatus::Playing && frames++ < 60 * 120) g.frame(kDt, PlayerCommand{}, FeelToggles{}, s, w);
    EXPECT_EQ(g.status(), GameStatus::Lost);
    EXPECT_EQ(g.player().health.hp(), 0);
    g.restart();
    EXPECT_EQ(g.status(), GameStatus::Playing);
    EXPECT_EQ(g.player().health.hp(), 100);
    EXPECT_EQ(g.wave(), 1);
}

TEST(LessonActionGame, EndlessNeverWins) {
    auto&          w = fresh_world();
    GameRules      r;
    r.endless = true;
    ActionGame     g({}, r);
    CombatSettings s;
    s.player.super_armor = SuperArmor::Always;   // keep the scripted player alive
    run(g, 60.0f, autopilot, FeelToggles{}, s, w);
    EXPECT_EQ(g.status(), GameStatus::Playing);
    EXPECT_GT(g.wave(), 3);
    EXPECT_EQ(g.view(s).waves_to_win, 0);
}

TEST(LessonActionGame, EnemyWindUpIsInterruptedWithoutSuperArmorAndNotWithIt) {
    // The player only swings at enemies that are winding up.
    CombatSettings plain;
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 8.0f, punish_windups, kOff, plain, w);
        EXPECT_GT(g.combat().enemy.interrupted, 0);
        EXPECT_EQ(g.combat().enemy.armor_absorbed, 0);
    }
    CombatSettings armored;
    armored.enemy.super_armor = SuperArmor::Attack;
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 8.0f, punish_windups, kOff, armored, w);
        EXPECT_EQ(g.combat().enemy.interrupted, 0);   // every wind-up went through
        EXPECT_GT(g.combat().enemy.armor_absorbed, 0);
        EXPECT_LT(g.player().health.hp(), 100);       // and the claws landed
    }
}

TEST(LessonActionGame, PlayerMotionCancelChainsAttacksThatHit) {
    CombatSettings on;
    on.player.motion_cancel = true;
    CombatSettings off;
    off.player.motion_cancel = false;
    on.enemy.flinch = off.enemy.flinch = false;   // keep the enemy in reach and swinging
    std::int64_t hits_on = 0;
    std::int64_t hits_off = 0;
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 6.0f, stand_and_swing, kOff, on, w);
        EXPECT_GT(g.combat().player.cancels, 0);
        hits_on = g.player_hits();
    }
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 6.0f, stand_and_swing, kOff, off, w);
        EXPECT_EQ(g.combat().player.cancels, 0);
        hits_off = g.player_hits();
    }
    EXPECT_GT(hits_on, hits_off);   // the recovery was cut short, so more swings fit in the same time
}

TEST(LessonActionGame, PlayerFlinchesWhenHitOnlyWithFlinchOn) {
    // The player keeps swinging the slow hammer at whatever is in reach, so enemy claws land during its attacks.
    auto hammer_swing = [](const GameView& v) {
        PlayerCommand c = stand_and_swing(v);
        if (v.fighters.front().weapon != std::string("hammer")) c.switch_weapon = true;
        return c;
    };
    CombatSettings on;
    on.enemy.flinch = false;
    CombatSettings off = on;
    off.player.flinch = false;
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 10.0f, hammer_swing, kOff, on, w);
        EXPECT_GT(g.combat().player.flinches, 0);
        EXPECT_GT(g.combat().player.interrupted, 0);
    }
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 10.0f, hammer_swing, kOff, off, w);
        EXPECT_EQ(g.combat().player.flinches, 0);
        EXPECT_EQ(g.combat().player.interrupted, 0);
        EXPECT_LT(g.player().health.hp(), 100);   // it was hit, just not staggered
    }
}

TEST(LessonActionGame, EnemyMotionCancelChainsAFollowUp) {
    CombatSettings on;
    on.enemy.motion_cancel = true;
    on.player.flinch       = false;
    CombatSettings off     = on;
    off.enemy.motion_cancel = false;
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 8.0f, stand, kOff, on, w);
        EXPECT_GT(g.combat().enemy.cancels, 0);
    }
    {
        auto&      w = fresh_world();
        ActionGame g;
        run(g, 8.0f, stand, kOff, off, w);
        EXPECT_EQ(g.combat().enemy.cancels, 0);
    }
}

TEST(LessonActionGame, PlayerSuperArmorAbsorbsEnemyHits) {
    auto&          w = fresh_world();
    ActionGame     g;
    CombatSettings s;
    s.player.super_armor = SuperArmor::Always;
    run(g, 8.0f, stand, FeelToggles{}, s, w);
    EXPECT_GT(g.combat().player.armor_absorbed, 0);
    EXPECT_EQ(g.combat().player.flinches, 0);
}

TEST(LessonActionGame, SameInputsSameGame) {
    CombatSettings s;
    s.enemy.super_armor  = SuperArmor::Attack;
    s.enemy.motion_cancel = true;
    auto play = [&](ActionGame& g) {
        auto& w = fresh_world();
        run(g, 20.0f, autopilot, FeelToggles{}, s, w);
    };
    ActionGame a;
    ActionGame b;
    play(a);
    play(b);
    EXPECT_EQ(a.player_hits(), b.player_hits());
    EXPECT_EQ(a.defeated(), b.defeated());
    EXPECT_EQ(a.score(), b.score());
    EXPECT_EQ(a.player().health.hp(), b.player().health.hp());
    EXPECT_TRUE(a.player().pos == b.player().pos);
    EXPECT_TRUE(a.world_seconds() == b.world_seconds());
    ASSERT_EQ(a.enemy_count(), b.enemy_count());
    for (std::size_t i = 0; i < a.enemy_count(); ++i) EXPECT_TRUE(a.enemy(i).pos == b.enemy(i).pos);
    EXPECT_EQ(a.combat().enemy.armor_absorbed, b.combat().enemy.armor_absorbed);
    EXPECT_EQ(a.combat().enemy.cancels, b.combat().enemy.cancels);
}
