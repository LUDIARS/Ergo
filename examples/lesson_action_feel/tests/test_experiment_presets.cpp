#include "experiment_presets.h"
#include <gtest/gtest.h>

using namespace lesson_action_feel;

TEST(LessonExperiments, TrapSelectionDoesNotAccumulateChangesOrChangeCombatBalance) {
    Options options;
    options.knockback_direction = KnockbackDirection::TargetBackward;
    const auto stop = experiment_setup(Experiment::LongStop, options);
    const auto push = experiment_setup(Experiment::ExcessiveKnockback, options);
    const auto shake = experiment_setup(Experiment::ExcessiveShake, options);
    const auto baseline = experiment_setup(Experiment::Baseline, options);
    EXPECT_GT(stop.feel.hit_stop_per_power, baseline.feel.hit_stop_per_power);
    EXPECT_FLOAT_EQ(push.feel.hit_stop_per_power, baseline.feel.hit_stop_per_power);
    EXPECT_GT(push.feel.knockback_per_power, baseline.feel.knockback_per_power);
    EXPECT_FLOAT_EQ(shake.feel.knockback_per_power, baseline.feel.knockback_per_power);
    EXPECT_GT(shake.presentation.shake_max, baseline.presentation.shake_max);
    EXPECT_EQ(stop.feel.player_weapons[1].damage, baseline.feel.player_weapons[1].damage);
    EXPECT_EQ(push.feel.knockback_direction, options.knockback_direction);
    EXPECT_FALSE(baseline.feel.rule);
}
