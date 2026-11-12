// Wave sizes and spawn points.
#include "wave_plan.h"

#include <gtest/gtest.h>

using namespace lesson_action_feel;

TEST(LessonWavePlan, FixedWavesThenNone) {
    const FeelTuning t;
    EXPECT_EQ(wave_size(1, false, t), 2);
    EXPECT_EQ(wave_size(2, false, t), 3);
    EXPECT_EQ(wave_size(3, false, t), 4);
    EXPECT_EQ(wave_size(4, false, t), 0);
    EXPECT_EQ(wave_size(0, false, t), 0);
    EXPECT_EQ(waves_to_win(false, t), 3);
}

TEST(LessonWavePlan, EndlessGrowsToACap) {
    const FeelTuning t;
    EXPECT_EQ(wave_size(1, true, t), 2);
    EXPECT_EQ(wave_size(4, true, t), 5);
    EXPECT_EQ(wave_size(50, true, t), t.endless_max_wave_size);
    EXPECT_EQ(waves_to_win(true, t), 0);
}

TEST(LessonWavePlan, SpawnsFarFromThePlayerInsideTheArena) {
    const FeelTuning t;
    const Vec2       player{2.0f, 2.0f};   // near the top-left corner
    const auto       s = spawn_points(4, player, t);
    ASSERT_EQ(s.size(), 4u);
    EXPECT_TRUE(s[0].pos == (Vec2{t.arena_width - 1.5f, t.arena_height - 1.5f}));   // the opposite corner first
    for (std::size_t i = 0; i < s.size(); ++i) {
        EXPECT_EQ(s[i].slot, static_cast<int>(i));
        EXPECT_GT((s[i].pos - player).length(), 5.0f);
        EXPECT_GE(s[i].pos.x, 0.0f);
        EXPECT_LE(s[i].pos.x, t.arena_width);
    }
    EXPECT_EQ(spawn_points(0, player, t).size(), 0u);
    EXPECT_EQ(spawn_points(99, player, t).size(), 10u);
}
