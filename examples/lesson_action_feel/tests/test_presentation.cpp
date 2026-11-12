// The effect registry and the Presentation: every effect is switchable and
// observable, purely visual effects never change the simulation, all-on vs
// all-off leaves the game's outcome the same, the boss blur envelope, motion
// vectors and the motion-blur streak.
#include "autopilot.h"
#include "boss_blur.h"
#include "effect_observations.h"
#include "motion_blur.h"
#include "session.h"

#include <gtest/gtest.h>

#include <functional>
#include <set>
#include <string>

using namespace lesson_action_feel;

namespace {

constexpr float kDt = 1.0f / 60.0f;
using Script = std::function<PlayerCommand(const GameView&)>;

ergo::world_time::Engine& fresh_world() {
    auto& w = ergo::world_time::Engine::instance();
    w.force_stop();
    return w;
}

void run(GameSession& g, float seconds, const Script& script, const FeelToggles& t, const CombatSettings& s) {
    auto& w = ergo::world_time::Engine::instance();
    for (int i = 0; i < static_cast<int>(seconds * 60.0f + 0.5f); ++i) g.frame(kDt, script(g.view(s, t)), t, s, w);
}

PlayerCommand stand(const GameView&) { return {}; }

GameRules boss_rules() {
    GameRules r;
    r.start_wave = FeelTuning{}.boss_every;
    return r;
}

/// Everything in the simulation that a purely visual effect must not change.
struct SimState {
    std::vector<float> xs, ys;
    std::vector<int>   hps;
    std::int64_t       score = 0, hits = 0, defeated = 0, flinches = 0, absorbed = 0, cancels = 0;
    double             world = 0, travel = 0, stop = 0, impulse = 0;
    int                wave = 0;
    bool operator==(const SimState& o) const {
        return xs == o.xs && ys == o.ys && hps == o.hps && score == o.score && hits == o.hits && defeated == o.defeated &&
               flinches == o.flinches && absorbed == o.absorbed && cancels == o.cancels && world == o.world &&
               travel == o.travel && stop == o.stop && impulse == o.impulse && wave == o.wave;
    }
};

SimState snapshot(const ActionGame& g) {
    SimState s;
    const auto add = [&](const Combatant& c) {
        s.xs.push_back(c.pos.x);
        s.ys.push_back(c.pos.y);
        s.hps.push_back(c.health.hp());
    };
    add(g.player());
    for (std::size_t i = 0; i < g.enemy_count(); ++i) add(g.enemy(i));
    s.score    = g.score();
    s.hits     = g.player_hits();
    s.defeated = g.defeated();
    s.flinches = g.combat().enemy.flinches + g.combat().player.flinches;
    s.absorbed = g.combat().enemy.armor_absorbed + g.combat().player.armor_absorbed;
    s.cancels  = g.combat().player.cancels + g.combat().enemy.cancels;
    s.world    = g.world_seconds();
    s.travel   = g.knockback_travel();
    s.stop     = g.observed().hit_stop_last_seconds;
    s.impulse  = g.observed().knockback_last_impulse;
    s.wave     = g.wave();
    return s;
}

/// A fixed play: 8 s of the scripted player, then 4 s standing still (to be hit), in endless waves; then a
/// boss fight. Returns the two sessions' simulation states.
std::pair<SimState, SimState> play(const FeelToggles& t, GameSession* keep_waves = nullptr, GameSession* keep_boss = nullptr) {
    CombatSettings s;
    fresh_world();
    GameRules endless;
    endless.endless = true;
    GameSession waves({}, endless);
    run(waves, 8.0f, autopilot, t, s);
    run(waves, 4.0f, stand, t, s);
    fresh_world();
    GameSession boss({}, boss_rules());
    run(boss, 12.0f, autopilot, t, s);
    if (keep_waves) *keep_waves = waves;
    if (keep_boss) *keep_boss = boss;
    return {snapshot(waves.game()), snapshot(boss.game())};
}

double observed_anywhere(Effect e, const GameSession& a, const GameSession& b) {
    return std::max(effect_observation(e, a), effect_observation(e, b));
}

} // namespace

TEST(LessonEffects, TheRegistryIsCompleteAndConsistent) {
    std::set<std::string> names;
    for (std::size_t i = 0; i < kEffectCount; ++i) {
        const EffectInfo& e = effect_table()[i];
        EXPECT_EQ(static_cast<std::size_t>(e.id), i);                       // table order = enum order
        EXPECT_TRUE(names.insert(std::string(e.name)).second);               // unique names
        EXPECT_EQ(std::string(e.observation).rfind("feel.", 0), 0u);
        EXPECT_TRUE(effect_by_name(e.name) == e.id);
        EXPECT_EQ(e.gameplay, e.id == Effect::HitStop || e.id == Effect::Knockback);   // only these two may change the game
    }
    EXPECT_FALSE(effect_by_name("nope").has_value());
    FeelToggles t;
    EXPECT_TRUE(t.all_on());
    t[Effect::MotionBlur] = false;
    EXPECT_FALSE(t.all_on());
    EXPECT_TRUE(FeelToggles::all(false) != FeelToggles::all(true));
}

TEST(LessonEffects, EveryObservationStaysZeroWhileItsEffectIsOffAndHappensWhenOn) {
    GameSession off_waves, off_boss, on_waves, on_boss;
    play(FeelToggles::all(false), &off_waves, &off_boss);
    play(FeelToggles::all(true), &on_waves, &on_boss);
    for (const EffectInfo& e : effect_table()) {
        EXPECT_TRUE(observed_anywhere(e.id, off_waves, off_boss) == 0.0);
        EXPECT_GT(observed_anywhere(e.id, on_waves, on_boss), 0.0);
    }
    // And with everything off the view carries nothing presentational.
    const GameView v = on_boss.view(CombatSettings{}, FeelToggles::all(false));
    EXPECT_TRUE(v.camera_offset.is_zero());
    EXPECT_TRUE(v.popups.empty());
    EXPECT_EQ(v.blur.kind, BlurKind::None);
    for (const FighterView& f : v.fighters) {
        EXPECT_FALSE(f.spark);
        EXPECT_FALSE(f.telegraph);
        EXPECT_TRUE(f.streak.is_zero());
    }
}

TEST(LessonEffects, PurelyVisualEffectsNeverChangeTheSimulation) {
    const auto reference = play(FeelToggles::all(true));
    FeelToggles only_gameplay = FeelToggles::all(false);
    only_gameplay[Effect::HitStop]   = true;
    only_gameplay[Effect::Knockback] = true;
    EXPECT_TRUE(play(only_gameplay) == reference);   // every visual effect off at once
    for (const EffectInfo& e : effect_table()) {
        if (e.gameplay) continue;
        FeelToggles t = FeelToggles::all(true);
        t[e.id]       = false;
        EXPECT_TRUE(play(t) == reference);           // and each one alone
    }
}

TEST(LessonEffects, AllOnAndAllOffPlayTheSameGameOutcome) {
    // Hit stop and knockback may change positions and timing; the rules may not: the same hits do the same
    // damage, defeat at the same hit, flinch / absorb the same way, and a whole game ends the same.
    auto  full = [](const FeelToggles& t) {
        auto&          w = fresh_world();
        ActionGame     g;
        CombatSettings s;
        std::int64_t   hits_at_first_defeat = -1;
        int            frames = 0;
        while (g.status() == GameStatus::Playing && frames++ < 60 * 180) {
            g.frame(kDt, autopilot(g.view(s)), t, s, w);
            if (hits_at_first_defeat < 0 && g.defeated() > 0) hits_at_first_defeat = g.player_hits();
        }
        return std::make_tuple(g.status(), g.defeated(), g.wave(), hits_at_first_defeat);
    };
    const auto on  = full(FeelToggles::all(true));
    const auto off = full(FeelToggles::all(false));
    EXPECT_EQ(std::get<0>(on), GameStatus::Won);
    EXPECT_TRUE(on == off);
    EXPECT_EQ(std::get<3>(on), 5);   // 100 HP / 20 damage, with or without the effects

    // Hit by hit, the rule outcome is the same with every effect on or off.
    const FeelTuning t;
    for (std::size_t w = 0; w < t.player_weapons.size(); ++w) {
        for (SuperArmor armor : {SuperArmor::Off, SuperArmor::Attack, SuperArmor::Always}) {
            for (bool flinch : {true, false}) {
                SideSettings es;
                es.super_armor = armor;
                es.flinch      = flinch;
                HitOutcome results[2];
                for (int k = 0; k < 2; ++k) {
                    Combatant p = make_combatant(0, Side::Player, {5, 5}, t.player_weapons[w], 100, 9, 0.5f);
                    Combatant e = make_combatant(1, Side::Enemy, {6, 5}, t.enemy_weapon, 100, 3, 0.5f);
                    SideCounters n;
                    Intent       in;
                    in.attack = true;
                    step_action(e, in, 0.001f, es, t, n);   // winding up
                    results[k] = resolve_hit(p, e, es, FeelToggles::all(k == 0), t, n);
                }
                EXPECT_EQ(results[0].damage, results[1].damage);
                EXPECT_EQ(results[0].killed, results[1].killed);
                EXPECT_EQ(results[0].flinched, results[1].flinched);
                EXPECT_EQ(results[0].absorbed, results[1].absorbed);
                EXPECT_EQ(results[0].interrupted, results[1].interrupted);
            }
        }
    }
}

TEST(LessonPresentation, CameraShakeFadesOnRealTimeEvenDuringAHitStop) {
    auto&          w = fresh_world();
    GameSession    g;
    CombatSettings s;
    FeelToggles    t = FeelToggles::all(false);
    t[Effect::HitStop]     = true;
    t[Effect::CameraShake] = true;
    int guard = 0;
    while (g.game().player_hits() == 0 && guard++ < 600) g.frame(kDt, autopilot(g.view(s, t)), t, s, w);
    ASSERT_TRUE(w.is_hit_stop());
    const float shake = g.fx().shake();
    EXPECT_NEAR(shake, 0.3f, 1e-5f);   // sword power 2 x 0.15
    EXPECT_EQ(g.frame(kDt, PlayerCommand{}, t, s, w).world_dt, 0.0f);
    EXPECT_LT(g.fx().shake(), shake);
    EXPECT_FALSE(g.view(s, t).camera_offset.is_zero());
}

// ---- boss blur envelope ----------------------------------------------------------

namespace {

Combatant boss_doing(const Weapon& w, AttackPhase phase, float progress) {
    const FeelTuning t;
    Combatant b = make_combatant(1, Side::Enemy, {10, 6}, w, t.boss_hp, t.boss_speed, t.boss_radius);
    b.is_boss   = true;
    b.action    = Action::Attack;
    b.phase     = phase;
    const float dur = phase == AttackPhase::WindUp ? w.windup : (phase == AttackPhase::Active ? w.active : w.recovery);
    b.phase_left    = dur * (1.0f - progress);
    return b;
}

} // namespace

TEST(LessonBossBlur, ZeroWithoutABossAction) {
    BossBlurEnvelope env;
    const BossBlurTuning t;
    EXPECT_EQ(env.update(nullptr, kDt, true, t).strength, 0.0f);
    Combatant idle = boss_doing(FeelTuning{}.boss_roar, AttackPhase::WindUp, 0.5f);
    idle.action    = Action::Ready;
    EXPECT_EQ(env.update(&idle, kDt, true, t).strength, 0.0f);
    EXPECT_TRUE(env.last_peak() == 0.0);
}

TEST(LessonBossBlur, ChargeRampsUpPeaksAtReleaseAndFadesOnRealTime) {
    const FeelTuning     ft;
    const BossBlurTuning t;
    BossBlurEnvelope     env;
    float last = 0.0f;
    for (int i = 0; i <= 20; ++i) {   // through the charge
        const Combatant b = boss_doing(ft.boss_charge, AttackPhase::WindUp, static_cast<float>(i) / 20.0f);
        const ScreenBlur s = env.update(&b, kDt, true, t);
        EXPECT_GE(s.strength, last);   // monotonic
        if (s.strength > 0.0f) EXPECT_EQ(s.kind, BlurKind::Zoom);
        last = s.strength;
    }
    EXPECT_NEAR(last, t.charge_peak, 1e-5f);
    const Combatant released = boss_doing(ft.boss_charge, AttackPhase::Active, 0.0f);
    EXPECT_FLOAT_EQ(env.update(&released, kDt, true, t).strength, t.release);   // the spike
    // The release's hit stop freezes the boss in its active phase; the blur still fades (real time).
    float during = t.release;
    for (int i = 0; i < 20; ++i) {
        const float s = env.update(&released, kDt, true, t).strength;
        EXPECT_LT(s, during);
        during = s;
    }
    Combatant done = released;
    done.action    = Action::Ready;
    for (int i = 0; i < 180; ++i) env.update(&done, kDt, true, t);
    EXPECT_EQ(env.update(&done, kDt, true, t).strength, 0.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(env.last_peak()), t.release);
}

TEST(LessonBossBlur, RoarRampsDuringItsActiveWindowRadially) {
    const FeelTuning     ft;
    const BossBlurTuning t;
    BossBlurEnvelope     env;
    const Combatant      w0 = boss_doing(ft.boss_roar, AttackPhase::WindUp, 1.0f);
    EXPECT_NEAR(env.update(&w0, kDt, true, t).strength, t.roar_windup, 1e-5f);
    float last = 0.0f;
    for (int i = 1; i <= 10; ++i) {
        const Combatant  b = boss_doing(ft.boss_roar, AttackPhase::Active, static_cast<float>(i) / 10.0f);
        const ScreenBlur s = env.update(&b, kDt, true, t);
        EXPECT_EQ(s.kind, BlurKind::Radial);
        EXPECT_GE(s.strength, last);
        last = s.strength;
    }
    EXPECT_NEAR(last, t.roar_peak, 1e-5f);
}

TEST(LessonBossBlur, OffMeansNothingAndNothingObserved) {
    const FeelTuning ft;
    BossBlurEnvelope env;
    const Combatant  b = boss_doing(ft.boss_charge, AttackPhase::WindUp, 0.9f);
    EXPECT_EQ(env.update(&b, kDt, false, BossBlurTuning{}).strength, 0.0f);
    Combatant done = b;
    done.action    = Action::Ready;
    env.update(&done, kDt, false, BossBlurTuning{});
    EXPECT_TRUE(env.last_peak() == 0.0);
}

TEST(LessonBossBlur, SameInputsSameEnvelope) {
    const FeelTuning ft;
    BossBlurEnvelope a, b;
    for (int i = 0; i <= 30; ++i) {
        const Combatant c = boss_doing(ft.boss_charge, i < 20 ? AttackPhase::WindUp : AttackPhase::Active,
                                       i < 20 ? static_cast<float>(i) / 20.0f : 0.0f);
        EXPECT_TRUE(a.update(&c, kDt, true, BossBlurTuning{}).strength == b.update(&c, kDt, true, BossBlurTuning{}).strength);
    }
}

TEST(LessonBossBlur, ABossFightDrivesIt) {
    auto&          w = fresh_world();
    GameSession    g({}, boss_rules());
    CombatSettings s;
    const FeelToggles t;
    float peak = 0.0f;
    bool  radial = false, zoom = false;
    for (int i = 0; i < 60 * 12; ++i) {
        g.frame(kDt, autopilot(g.view(s, t)), t, s, w);
        const ScreenBlur b = g.view(s, t).blur;
        peak   = std::max(peak, b.strength);
        radial = radial || b.kind == BlurKind::Radial;
        zoom   = zoom || b.kind == BlurKind::Zoom;
    }
    EXPECT_TRUE(radial);
    EXPECT_TRUE(zoom);
    EXPECT_FLOAT_EQ(peak, 1.0f);
}

// ---- motion vectors and the motion-blur streak --------------------------------------

TEST(LessonMotion, VectorsAreTheFrameToFrameDisplacement) {
    auto&          w = fresh_world();
    GameSession    g;
    CombatSettings s;
    const FeelToggles t = FeelToggles::all(false);
    g.frame(kDt, PlayerCommand{}, t, s, w);
    for (const FighterView& f : g.view(s, t).fighters) EXPECT_TRUE(f.motion.is_zero());   // first frame: nothing to compare
    PlayerCommand right;
    right.intent.move = {1, 0};
    const float x0 = g.game().player().pos.x;
    g.frame(kDt, right, t, s, w);
    const Vec2 m = g.view(s, t).fighters.front().motion;
    EXPECT_FLOAT_EQ(m.x, g.game().player().pos.x - x0);
    EXPECT_NEAR(m.x, 9.0f / 60.0f, 1e-5f);
    EXPECT_FLOAT_EQ(m.y, 0.0f);
    EXPECT_TRUE(g.view(s, t).camera_motion.is_zero());   // the camera does not scroll
}

TEST(LessonMotion, KnockbackGivesTheEnemyABigVectorAndATrail) {
    auto&          w = fresh_world();
    GameSession    g;
    CombatSettings s;
    FeelToggles    t = FeelToggles::all(false);
    t[Effect::Knockback]  = true;
    t[Effect::MotionBlur] = true;
    int guard = 0;
    while (g.game().player_hits() == 0 && guard++ < 600) g.frame(kDt, autopilot(g.view(s, t)), t, s, w);
    ASSERT_EQ(g.game().player_hits(), 1);
    g.frame(kDt, PlayerCommand{}, t, s, w);   // the push shows up in the next displacement
    const int  hit = g.fx().last_hit_target();
    const Vec2 m   = g.fx().motion_of(hit);
    EXPECT_GT(m.length(), 9.0f / 60.0f);   // faster than the player walks
    bool trail = false;
    for (const FighterView& f : g.view(s, t).fighters)
        if (f.id == hit) {
            trail = !f.streak.is_zero();
            EXPECT_LT(f.streak.dot(m), 0.0f);   // behind the enemy
        }
    EXPECT_TRUE(trail);
    EXPECT_GT(effect_observation(Effect::MotionBlur, g), 0.0);
}

TEST(LessonMotion, AHitStopFreezesTheVectorsToZero) {
    auto&          w = fresh_world();
    GameSession    g;
    CombatSettings s;
    FeelToggles    t;   // everything on
    int guard = 0;
    while (g.game().player_hits() == 0 && guard++ < 600) g.frame(kDt, autopilot(g.view(s, t)), t, s, w);
    ASSERT_TRUE(w.is_hit_stop());
    ASSERT_EQ(g.frame(kDt, autopilot(g.view(s, t)), t, s, w).world_dt, 0.0f);
    for (const FighterView& f : g.view(s, t).fighters) {
        EXPECT_TRUE(f.motion.is_zero());
        EXPECT_TRUE(f.streak.is_zero());   // the freeze reads as crisp
    }
}

TEST(LessonMotion, RestartsAndSpawnsDoNotJump) {
    auto&          w = fresh_world();
    GameSession    g;
    CombatSettings s;
    const FeelToggles t;
    run(g, 3.0f, autopilot, t, s);
    g.restart();   // everybody back to their start, ids reused
    g.frame(kDt, PlayerCommand{}, t, s, w);
    for (const FighterView& f : g.view(s, t).fighters) EXPECT_LT(f.motion.length(), 0.2f);
    // Through whole waves (spawns at the far edges) no vector is ever larger than a knockback can make.
    float largest = 0.0f;
    for (int i = 0; i < 60 * 30; ++i) {
        g.frame(kDt, autopilot(g.view(s, t)), t, s, w);
        for (const FighterView& f : g.view(s, t).fighters) largest = std::max(largest, f.motion.length());
    }
    EXPECT_LT(largest, 1.0f);
}

TEST(LessonMotion, SameInputsSameVectors) {
    const FeelToggles t;
    CombatSettings    s;
    std::vector<float> a, b;
    for (auto* out : {&a, &b}) {
        fresh_world();
        GameSession g;
        for (int i = 0; i < 60 * 6; ++i) {
            g.frame(kDt, autopilot(g.view(s, t)), t, s, ergo::world_time::Engine::instance());
            for (const FighterView& f : g.view(s, t).fighters) out->push_back(f.motion.x + 7.0f * f.motion.y);
        }
    }
    EXPECT_TRUE(a == b);
}

TEST(LessonMotionStreak, LengthDirectionThresholdAndClamp) {
    const MotionBlurTuning t;   // shutter 5, min 0.8, max 3
    EXPECT_FALSE(motion_streak({}, t).any());
    EXPECT_FALSE(motion_streak({9.0f / 60.0f, 0}, t).any());   // walking: 0.75 < 0.8
    const Streak s = motion_streak({0.2f, 0}, t);              // 1.0
    ASSERT_TRUE(s.any());
    EXPECT_FLOAT_EQ(s.length, 1.0f);
    EXPECT_TRUE(s.dir == (Vec2{-1, 0}));                       // exactly behind
    EXPECT_EQ(s.samples, 2);
    const Streak d = motion_streak({0.3f, -0.4f}, t);          // 0.5 x 5 = 2.5, diagonal
    EXPECT_FLOAT_EQ(d.length, 2.5f);
    EXPECT_NEAR(d.dir.x, -0.6f, 1e-6f);
    EXPECT_NEAR(d.dir.y, 0.8f, 1e-6f);
    EXPECT_FLOAT_EQ(motion_streak({2.0f, 0}, t).length, 3.0f);   // clamped
    EXPECT_FLOAT_EQ(motion_streak({0.4f, 0}, t).length, 2.0f * motion_streak({0.2f, 0}, t).length);   // proportional
}
