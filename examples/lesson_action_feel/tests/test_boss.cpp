// The boss: its pattern, the roar (a forced stagger, armored itself), the charge
// (interruptible or not under each enemy-side setting, the boss's poise) and the
// release (damage, knockback, hit stop); the boss wave and the win.
#include "action_game.h"
#include "autopilot.h"
#include "boss_brain.h"
#include "combat_rules.h"

#include <gtest/gtest.h>

using namespace lesson_action_feel;

namespace {

const FeelTuning kTuning{};

Combatant make_boss(Vec2 pos) {
    Combatant b = make_combatant(1, Side::Enemy, pos, kTuning.boss_roar, kTuning.boss_hp, kTuning.boss_speed,
                                 kTuning.boss_radius);
    b.is_boss          = true;
    b.mass             = kTuning.boss_mass;
    b.min_flinch_power = kTuning.boss_min_flinch_power;
    b.techniques       = {kTuning.boss_roar, kTuning.boss_charge};
    return b;
}

Combatant make_player(Vec2 pos, std::size_t weapon = 0) {
    return make_combatant(0, Side::Player, pos, kTuning.player_weapons[weapon], kTuning.player_hp, kTuning.player_speed,
                          kTuning.radius);
}

/// Start technique `t` and step it into `phase`.
void into(Combatant& c, int technique, AttackPhase phase, const SideSettings& s) {
    SideCounters n;
    Intent       in;
    in.attack    = true;
    in.technique = technique;
    in.aim       = {1, 0};
    step_action(c, in, 0.001f, s, kTuning, n);
    ASSERT_EQ(c.action, Action::Attack);
    while (c.phase != phase) step_action(c, Intent{}, 0.01f, s, kTuning, n);
}

} // namespace

TEST(LessonBoss, PatternIsRoarChargeCharge) {
    BrainState b;
    std::vector<int> seen;
    for (int i = 0; i < 6; ++i) {
        seen.push_back(boss_next_technique(b));
        ++b.chain;
    }
    EXPECT_EQ(seen, (std::vector<int>{kBossRoar, kBossCharge, kBossCharge, kBossRoar, kBossCharge, kBossCharge}));
}

TEST(LessonBoss, TheBrainRoarsWhenThePlayerIsWithinReachAndWaitsItsCooldown) {
    Combatant  boss   = make_boss({10, 6});
    Combatant  player = make_player({14, 6});   // 4 units: inside the roar (6), outside the charge
    BrainState brain;
    brain.cooldown = 0.0f;
    Intent in = think_boss(boss, player, brain, 0.016f, kTuning);
    EXPECT_TRUE(in.attack);
    EXPECT_EQ(in.technique, kBossRoar);
    EXPECT_GT(brain.cooldown, 0.0f);
    // Next is a charge: too far for it, so it walks.
    in = think_boss(boss, player, brain, 0.016f, kTuning);
    EXPECT_FALSE(in.attack);
    EXPECT_FALSE(in.move.is_zero());
}

TEST(LessonBoss, RoarStaggersThePlayerEvenWithFlinchOffButNotThroughSuperArmor) {
    Combatant    boss = make_boss({10, 6});
    SideSettings enemy;
    into(boss, kBossRoar, AttackPhase::Active, enemy);

    for (bool flinch : {true, false}) {
        Combatant    p = make_player({14, 6});
        SideSettings ps;
        ps.flinch = flinch;
        SideCounters n;
        const HitOutcome r = resolve_roar(boss, p, ps, n);
        EXPECT_TRUE(r.flinched);
        EXPECT_EQ(p.action, Action::Flinch);
        EXPECT_FLOAT_EQ(p.flinch_left, kTuning.boss_roar.stagger);
        EXPECT_EQ(p.health.hp(), 100);        // no damage
        EXPECT_TRUE(p.push_velocity.is_zero());   // no knockback
        EXPECT_TRUE(r.hit_stop_seconds == 0.0f);
        EXPECT_EQ(n.flinches, 1);
    }
    Combatant    armored = make_player({14, 6});
    SideSettings ps;
    ps.super_armor = SuperArmor::Always;
    SideCounters n;
    EXPECT_TRUE(resolve_roar(boss, armored, ps, n).absorbed);
    EXPECT_EQ(armored.action, Action::Ready);
    EXPECT_EQ(n.armor_absorbed, 1);
}

TEST(LessonBoss, TheRoarIsArmoredWhateverTheEnemySideSays) {
    Combatant    boss = make_boss({10, 6});
    SideSettings enemy;   // flinch on, super armor off
    into(boss, kBossRoar, AttackPhase::WindUp, enemy);
    EXPECT_TRUE(armor_applies(boss, enemy));
    Combatant    p = make_player({8, 6}, 1);   // the hammer
    SideCounters n;
    const HitOutcome h = resolve_hit(p, boss, enemy, FeelToggles{}, kTuning, n);
    EXPECT_TRUE(h.absorbed);
    EXPECT_EQ(boss.action, Action::Attack);
    EXPECT_EQ(boss.health.hp(), 400 - 35);
}

TEST(LessonBoss, ChargeInterruptibilityFollowsTheEnemySideAndThePoise) {
    struct Case {
        const char*  what;
        std::size_t  weapon;
        SideSettings enemy;
        bool         interrupted;
    };
    SideSettings plain;
    SideSettings armor;
    armor.super_armor = SuperArmor::Attack;
    SideSettings no_flinch;
    no_flinch.flinch = false;
    const Case cases[] = {
        {"hammer (power 6 >= poise 4), flinch on, no armor", 1, plain, true},
        {"sword (power 2 < poise 4)", 0, plain, false},
        {"hammer against enemy super armor (attack)", 1, armor, false},
        {"hammer with enemy flinch off", 1, no_flinch, false},
    };
    for (const Case& c : cases) {
        Combatant boss = make_boss({10, 6});
        into(boss, kBossCharge, AttackPhase::WindUp, c.enemy);
        Combatant    p = make_player({8, 6}, c.weapon);
        SideCounters n;
        const HitOutcome h = resolve_hit(p, boss, c.enemy, FeelToggles{}, kTuning, n);
        EXPECT_EQ(h.interrupted, c.interrupted);
        EXPECT_EQ(boss.action == Action::Flinch, c.interrupted);
        EXPECT_EQ(boss.action == Action::Attack, !c.interrupted);
    }
}

TEST(LessonBoss, TheReleaseHitsHard) {
    Combatant    boss = make_boss({10, 6});
    SideSettings enemy;
    into(boss, kBossCharge, AttackPhase::Active, enemy);
    Combatant    p = make_player({12, 6});
    EXPECT_TRUE(swing_touches(boss, p));
    SideCounters n;
    const HitOutcome h = resolve_hit(boss, p, SideSettings{}, FeelToggles{}, kTuning, n);
    EXPECT_EQ(h.damage, 40);
    EXPECT_EQ(p.health.hp(), 60);
    EXPECT_FLOAT_EQ(h.impulse, 9.0f * 6.0f);            // power 9 x 6, the player's mass is 1
    EXPECT_FLOAT_EQ(h.hit_stop_seconds, 9.0f / 24.0f);  // 0.375 s
    EXPECT_TRUE(h.flinched);
    EXPECT_GT(p.push_velocity.x, 50.0f);                 // away from the boss
}

TEST(LessonBoss, KnockbackMovesTheHeavyBossLess) {
    Combatant    boss = make_boss({10, 6});
    Combatant    e    = make_combatant(2, Side::Enemy, {10, 6}, kTuning.enemy_weapon, 100, 3.5f, 0.5f);
    Combatant    p    = make_player({8, 6}, 1);
    SideCounters n;
    resolve_hit(p, boss, SideSettings{}, FeelToggles{}, kTuning, n);
    resolve_hit(p, e, SideSettings{}, FeelToggles{}, kTuning, n);
    EXPECT_FLOAT_EQ(boss.push_velocity.x * kTuning.boss_mass, e.push_velocity.x);
}

TEST(LessonBoss, TheBossWaveAndTheWin) {
    auto& w = ergo::world_time::Engine::instance();
    w.force_stop();
    GameRules r;
    r.start_wave = kTuning.boss_every;
    ActionGame g({}, r);
    ASSERT_EQ(g.enemy_count(), 1u);
    ASSERT_NE(g.boss(), nullptr);
    EXPECT_EQ(g.boss()->health.hp(), 400);
    CombatSettings s;
    bool roared = false, charged = false;
    int  frames = 0;
    while (g.status() == GameStatus::Playing && frames++ < 60 * 90) {
        g.frame(1.0f / 60.0f, autopilot(g.view(s)), FeelToggles{}, s, w);
        if (const Combatant* b = g.boss(); b && b->action == Action::Attack) (b->weapon.roar ? roared : charged) = true;
    }
    EXPECT_TRUE(roared);
    EXPECT_TRUE(charged);
    EXPECT_EQ(g.status(), GameStatus::Won);   // the boss wave is the last one
    EXPECT_EQ(g.defeated(), 1);
}
