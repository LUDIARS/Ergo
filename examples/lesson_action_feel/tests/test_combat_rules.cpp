// The combat rules shared by both sides: action phases, motion cancel,
// flinch, super armor, knockback direction and the damage / power split.
#include "combat_rules.h"

#include <gtest/gtest.h>

using namespace lesson_action_feel;

namespace {

const FeelTuning kTuning{};

Combatant fighter(Side side, Vec2 pos, const Weapon& w, std::int32_t hp = 100) {
    return make_combatant(side == Side::Player ? 0 : 1, side, pos, w, hp, side == Side::Player ? 9.0f : 3.0f, 0.5f);
}

Combatant player_with(std::size_t weapon) { return fighter(Side::Player, {5, 5}, kTuning.player_weapons[weapon]); }
Combatant enemy_at(Vec2 pos, std::int32_t hp = 100) { return fighter(Side::Enemy, pos, kTuning.enemy_weapon, hp); }

Intent attack_toward(Vec2 dir) {
    Intent in;
    in.aim    = dir;
    in.attack = true;
    return in;
}

/// Step until the fighter is no longer in Action::Attack; returns world seconds spent.
float run_out_attack(Combatant& c, const SideSettings& s, SideCounters& n, float dt = 0.01f) {
    float t = 0.0f;
    while (c.action == Action::Attack && t < 10.0f) {
        step_action(c, Intent{}, dt, s, kTuning, n);
        t += dt;
    }
    return t;
}

void start_windup(Combatant& c, const SideSettings& s, SideCounters& n) {
    step_action(c, attack_toward(c.facing), 0.001f, s, kTuning, n);
    ASSERT_EQ(c.action, Action::Attack);
    ASSERT_EQ(c.phase, AttackPhase::WindUp);
}

} // namespace

TEST(LessonCombatRules, ReadyFighterWalksAndFacesWhereItWalks) {
    Combatant    p = player_with(0);
    SideCounters n;
    Intent       in;
    in.move = {0, 2};   // any length: it is normalised
    step_action(p, in, 0.1f, SideSettings{}, kTuning, n);
    EXPECT_NEAR(p.pos.y, 5.0f + 0.9f, 1e-5f);
    EXPECT_NEAR(p.pos.x, 5.0f, 1e-6f);
    EXPECT_TRUE(p.facing == (Vec2{0, 1}));
}

TEST(LessonCombatRules, AttackRunsWindUpActiveRecoveryThenReady) {
    Combatant    p = player_with(0);
    SideCounters n;
    SideSettings s;
    s.motion_cancel = false;
    step_action(p, attack_toward({1, 0}), 0.001f, s, kTuning, n);
    ASSERT_EQ(p.action, Action::Attack);
    EXPECT_EQ(p.phase, AttackPhase::WindUp);
    const Weapon& w = kTuning.player_weapons[0];
    const float   t = run_out_attack(p, s, n);
    EXPECT_NEAR(t, w.windup + w.active + w.recovery, 0.02f);
    EXPECT_EQ(p.action, Action::Ready);

    // Committed: walking during the attack does nothing until it ends.
    step_action(p, attack_toward({1, 0}), 0.001f, s, kTuning, n);
    Intent walk;
    walk.move        = {0, 1};
    const Vec2 start = p.pos;
    for (int i = 0; i < 5; ++i) step_action(p, walk, 0.01f, s, kTuning, n);
    EXPECT_TRUE(p.pos == start);
}

TEST(LessonCombatRules, AnEarlyAttackPressIsBufferedBriefly) {
    Combatant    p = player_with(0);
    SideCounters n;
    SideSettings s;
    s.motion_cancel = false;
    step_action(p, attack_toward({1, 0}), 0.001f, s, kTuning, n);
    const Weapon& w = kTuning.player_weapons[0];
    // Run until 0.05 s before the recovery ends, press again, then let it end: a new attack starts by itself.
    float t = 0.001f;
    while (t < w.windup + w.active + w.recovery - 0.05f) {
        step_action(p, Intent{}, 0.01f, s, kTuning, n);
        t += 0.01f;
    }
    step_action(p, attack_toward({1, 0}), 0.01f, s, kTuning, n);
    for (int i = 0; i < 6; ++i) step_action(p, Intent{}, 0.01f, s, kTuning, n);
    EXPECT_EQ(p.action, Action::Attack);
    EXPECT_EQ(p.phase, AttackPhase::WindUp);
    EXPECT_EQ(n.cancels, 0);   // the first attack ran to its end: no cancel
}

TEST(LessonCombatRules, DamageDrivesHpAndPowerDrivesImpulseStopAndShake) {
    FeelToggles  on;
    SideCounters n;
    SideSettings s;
    for (std::size_t w = 0; w < kTuning.player_weapons.size(); ++w) {
        Combatant        p   = player_with(w);
        Combatant        e   = enemy_at({6, 5});
        const Weapon&    wp  = kTuning.player_weapons[w];
        const HitOutcome hit = resolve_hit(p, e, s, on, kTuning, n);
        EXPECT_EQ(e.health.hp(), 100 - wp.damage);
        EXPECT_FLOAT_EQ(hit.impulse, wp.power * kTuning.knockback_per_power);
        EXPECT_FLOAT_EQ(hit.hit_stop_seconds, wp.power / 24.0f);
        EXPECT_FLOAT_EQ(hit.shake, std::min(wp.power * kTuning.shake_per_power, kTuning.shake_max));
        EXPECT_FLOAT_EQ(e.push_velocity.x, hit.impulse / kTuning.mass);
    }
    // The two weapons really differ in both numbers.
    EXPECT_NE(kTuning.player_weapons[0].damage, kTuning.player_weapons[1].damage);
    EXPECT_NE(kTuning.player_weapons[0].power, kTuning.player_weapons[1].power);
}

TEST(LessonCombatRules, FeelSwitchesOffLeaveOnlyTheDamage) {
    Combatant        p = player_with(1);
    Combatant        e = enemy_at({6, 5});
    SideCounters     n;
    const HitOutcome hit = resolve_hit(p, e, SideSettings{}, FeelToggles{false, false, false}, kTuning, n);
    EXPECT_EQ(e.health.hp(), 65);
    EXPECT_TRUE(hit.impulse == 0.0f);
    EXPECT_TRUE(hit.hit_stop_seconds == 0.0f);
    EXPECT_TRUE(hit.shake == 0.0f);
    EXPECT_TRUE(e.push_velocity.is_zero());
    EXPECT_TRUE(hit.flinched);   // flinch is a combat setting, not a feel effect
}

TEST(LessonCombatRules, KnockbackDirectionRules) {
    // The enemy stands right of the player and faces right (the player hits it from behind).
    Combatant p = player_with(0);
    Combatant e = enemy_at({6, 5});
    e.facing    = {1, 0};
    EXPECT_TRUE(knockback_direction(p, e, KnockbackDirection::AwayFromAttacker) == (Vec2{1, 0}));
    EXPECT_TRUE(knockback_direction(p, e, KnockbackDirection::TargetBackward) == (Vec2{-1, 0}));   // toward the player
    e.facing = {-1, 0};   // now it faces the player: both rules push it away
    EXPECT_TRUE(knockback_direction(p, e, KnockbackDirection::TargetBackward) == (Vec2{1, 0}));
    // Same spot: the attacker's facing decides.
    Combatant same = enemy_at(p.pos);
    p.facing       = {0, -1};
    EXPECT_TRUE(knockback_direction(p, same, KnockbackDirection::AwayFromAttacker) == (Vec2{0, -1}));
}

TEST(LessonCombatRules, SwingTouchesInsideReachAndArcOnly) {
    Combatant p = player_with(0);   // reach 1.8, arc_cos 0.34
    p.facing    = {1, 0};
    EXPECT_TRUE(swing_touches(p, enemy_at({7.2f, 5})));    // 2.2 <= 1.8 + 0.5
    EXPECT_FALSE(swing_touches(p, enemy_at({7.4f, 5})));   // out of reach
    EXPECT_FALSE(swing_touches(p, enemy_at({4, 5})));      // behind
    EXPECT_FALSE(swing_touches(p, enemy_at({5.2f, 6.5f}))); // outside the arc (cos ~ 0.13)
}

TEST(LessonCombatRules, FlinchOnInterruptsAWindUp) {
    Combatant    p = player_with(0);
    Combatant    e = enemy_at({6, 5});
    SideCounters n;
    SideSettings es;
    start_windup(e, es, n);
    const HitOutcome hit = resolve_hit(p, e, es, FeelToggles{}, kTuning, n);
    EXPECT_TRUE(hit.flinched);
    EXPECT_TRUE(hit.interrupted);
    EXPECT_EQ(e.action, Action::Flinch);
    EXPECT_FLOAT_EQ(e.flinch_left, es.flinch_seconds);
    EXPECT_EQ(n.flinches, 1);
    EXPECT_EQ(n.interrupted, 1);

    // A flinching fighter neither walks nor attacks, then is Ready again after flinch_seconds.
    Intent all;
    all.move   = {1, 0};
    all.attack = true;
    const Vec2 at = e.pos;
    step_action(e, all, es.flinch_seconds * 0.5f, es, kTuning, n);
    EXPECT_EQ(e.action, Action::Flinch);
    EXPECT_TRUE(e.pos == at);
    step_action(e, Intent{}, es.flinch_seconds * 0.6f, es, kTuning, n);
    EXPECT_EQ(e.action, Action::Ready);
}

TEST(LessonCombatRules, FlinchOffLetsTheActionContinueButStillPushes) {
    Combatant    p = player_with(0);
    Combatant    e = enemy_at({6, 5});
    SideCounters n;
    SideSettings es;
    es.flinch = false;
    start_windup(e, es, n);
    const HitOutcome hit = resolve_hit(p, e, es, FeelToggles{}, kTuning, n);
    EXPECT_FALSE(hit.flinched);
    EXPECT_FALSE(hit.interrupted);
    EXPECT_EQ(e.action, Action::Attack);
    EXPECT_EQ(e.phase, AttackPhase::WindUp);
    EXPECT_GT(hit.impulse, 0.0f);   // knockback is the feel switch, independent of flinch
    EXPECT_EQ(n.flinches, 0);
}

TEST(LessonCombatRules, SuperArmorDuringAttackAbsorbsAWindUpHit) {
    Combatant    p = player_with(1);
    Combatant    e = enemy_at({6, 5});
    SideCounters n;
    SideSettings es;
    es.super_armor = SuperArmor::Attack;
    start_windup(e, es, n);
    EXPECT_TRUE(armor_applies(e, es));
    const HitOutcome hit = resolve_hit(p, e, es, FeelToggles{}, kTuning, n);
    EXPECT_TRUE(hit.absorbed);
    EXPECT_FALSE(hit.flinched);               // super armor beats flinch
    EXPECT_FALSE(hit.interrupted);
    EXPECT_TRUE(hit.impulse == 0.0f);         // and resists the push
    EXPECT_TRUE(e.push_velocity.is_zero());
    EXPECT_GT(hit.hit_stop_seconds, 0.0f);    // the hit stop still happens
    EXPECT_GT(hit.shake, 0.0f);
    EXPECT_EQ(e.health.hp(), 65);             // and so does the damage
    EXPECT_EQ(e.action, Action::Attack);
    EXPECT_EQ(n.armor_absorbed, 1);
    EXPECT_EQ(n.flinches, 0);
}

TEST(LessonCombatRules, SuperArmorDuringAttackDoesNotCoverReadyOrRecovery) {
    SideSettings es;
    es.super_armor = SuperArmor::Attack;
    Combatant    e = enemy_at({6, 5});
    EXPECT_FALSE(armor_applies(e, es));   // Ready
    SideCounters n;
    start_windup(e, es, n);
    while (e.phase != AttackPhase::Recovery) step_action(e, Intent{}, 0.01f, es, kTuning, n);
    EXPECT_FALSE(armor_applies(e, es));   // the recovery is the punish window
    Combatant        p   = player_with(0);
    const HitOutcome hit = resolve_hit(p, e, es, FeelToggles{}, kTuning, n);
    EXPECT_TRUE(hit.flinched);
    EXPECT_TRUE(hit.interrupted);
}

TEST(LessonCombatRules, SuperArmorAlwaysCoversAReadyFighterButNotDeath) {
    SideSettings es;
    es.super_armor = SuperArmor::Always;
    Combatant    p = player_with(1);   // 35 damage
    Combatant    e = enemy_at({6, 5}, 50);
    SideCounters n;
    EXPECT_TRUE(resolve_hit(p, e, es, FeelToggles{}, kTuning, n).absorbed);
    const HitOutcome second = resolve_hit(p, e, es, FeelToggles{}, kTuning, n);   // 50 - 35 - 35 < 0
    EXPECT_TRUE(second.killed);
    EXPECT_FALSE(second.absorbed);
    EXPECT_EQ(e.action, Action::Dead);
}

TEST(LessonCombatRules, MotionCancelOnlyFromTheRecoveryOfAnAttackThatHit) {
    Combatant    p = player_with(0);
    SideCounters n;
    SideSettings ps;
    ps.motion_cancel = true;
    step_action(p, attack_toward({1, 0}), 0.001f, ps, kTuning, n);
    EXPECT_FALSE(can_cancel(p, ps));   // wind-up
    while (p.phase != AttackPhase::Recovery) step_action(p, Intent{}, 0.01f, ps, kTuning, n);
    EXPECT_FALSE(can_cancel(p, ps));   // a whiff cannot be cancelled
    p.attack_landed = true;
    EXPECT_TRUE(can_cancel(p, ps));
    SideSettings off = ps;
    off.motion_cancel = false;
    EXPECT_FALSE(can_cancel(p, off));

    step_action(p, attack_toward({0, 1}), 0.01f, ps, kTuning, n);   // cancel into a new attack
    EXPECT_EQ(n.cancels, 1);
    EXPECT_EQ(p.action, Action::Attack);
    EXPECT_EQ(p.phase, AttackPhase::WindUp);
    EXPECT_TRUE(p.facing == (Vec2{0, 1}));
}

TEST(LessonCombatRules, MotionCancelIntoAMove) {
    Combatant    p = player_with(0);
    SideCounters n;
    SideSettings ps;
    ps.motion_cancel = true;
    step_action(p, attack_toward({1, 0}), 0.001f, ps, kTuning, n);
    while (p.phase != AttackPhase::Recovery) step_action(p, Intent{}, 0.01f, ps, kTuning, n);
    p.attack_landed = true;
    Intent walk;
    walk.move        = {0, -1};
    const float y0   = p.pos.y;
    step_action(p, walk, 0.1f, ps, kTuning, n);
    EXPECT_EQ(n.cancels, 1);
    EXPECT_EQ(p.action, Action::Ready);
    EXPECT_LT(p.pos.y, y0);
}

TEST(LessonCombatRules, AFlinchCannotBeCancelled) {
    Combatant    p = player_with(0);
    Combatant    e = enemy_at({6, 5});
    SideCounters n;
    SideSettings es;
    es.motion_cancel = true;
    start_windup(e, es, n);
    e.attack_landed = true;
    resolve_hit(p, e, es, FeelToggles{}, kTuning, n);
    ASSERT_EQ(e.action, Action::Flinch);
    EXPECT_FALSE(can_cancel(e, es));
    step_action(e, attack_toward({-1, 0}), 0.01f, es, kTuning, n);
    EXPECT_EQ(e.action, Action::Flinch);
    EXPECT_EQ(n.cancels, 0);
}
