#include "combat_rules.h"
#include <gtest/gtest.h>
#include <limits>

using namespace lesson_action_feel;

TEST(LessonLiveRule, ReplacesFeelWithoutChangingDamageOrBypassingToggles) {
    FeelTuning tuning;
    int calls = 0;
    tuning.rule = [&calls](RuleKind kind, float, float) { ++calls; return kind == RuleKind::HitStop ? .1f : 10.0f; };
    const auto& weapon = tuning.player_weapons[1];
    auto player = make_combatant(0, Side::Player, {5, 5}, weapon, 100, 9, .5f);
    auto enemy = make_combatant(1, Side::Enemy, {6, 5}, tuning.enemy_weapon, 100, 3.5f, .5f);
    SideCounters counters;
    const auto hit = resolve_hit(player, enemy, SideSettings{}, FeelToggles::all(true), tuning, counters);
    EXPECT_EQ(hit.damage, weapon.damage);
    EXPECT_EQ(enemy.health.hp(), 100 - weapon.damage);
    EXPECT_FLOAT_EQ(hit.hit_stop_seconds, .1f);
    EXPECT_EQ(calls, 2);
    const auto off = resolve_hit(player, enemy, SideSettings{}, FeelToggles::all(false), tuning, counters);
    EXPECT_FLOAT_EQ(off.impulse, 0);
    EXPECT_FLOAT_EQ(off.hit_stop_seconds, 0);
    EXPECT_EQ(calls, 2);
}

TEST(LessonLiveRule, RejectsInvalidOutputRatherThanPropagatingItIntoSimulation) {
    for (const float value : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(), -1.0f, 201.0f}) {
        const RuleFunction rule = [value](RuleKind, float, float) { return value; };
        bool rejected = false;
        try { (void)evaluate_rule(rule, RuleKind::Knockback, 3, 18); }
        catch (const std::domain_error&) { rejected = true; }
        EXPECT_TRUE(rejected);
    }
}

TEST(LessonLiveRule, EmptyExtensionPreservesBaseline) {
    EXPECT_FLOAT_EQ(evaluate_rule({}, RuleKind::HitStop, 6, .25f), .25f);
    EXPECT_FLOAT_EQ(evaluate_rule({}, RuleKind::Knockback, 6, 36), 36);
}
