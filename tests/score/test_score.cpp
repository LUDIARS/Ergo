#include "ergo/score/score.h"

#include "gtest/gtest.h"

using ergo::score::Config;
using ergo::score::Score;

TEST(Score, StartsAtZero) {
    Score s;
    EXPECT_EQ(s.score(), 0);
    EXPECT_EQ(s.high_score(), 0);
}

TEST(Score, AddBaseWithoutCombo) {
    Config cfg;
    cfg.combo_multiplier = false;
    Score s(cfg);
    auto applied = s.add(100);
    EXPECT_EQ(applied, 100);
    EXPECT_EQ(s.score(), 100);
}

TEST(Score, ComboMultiplierIncreasesApplied) {
    Config cfg;
    cfg.combo_multiplier = true;
    cfg.combo_factor = 0.1f;
    Score s(cfg);
    auto a = s.add(100, 0);  // m = 1.0 (no combo)
    EXPECT_EQ(a, 100);
    auto b = s.add(100, 10); // m = 2.0
    EXPECT_EQ(b, 200);
    EXPECT_EQ(s.score(), 300);
}

TEST(Score, MultiplierCap) {
    Config cfg;
    cfg.combo_multiplier = true;
    cfg.combo_factor = 1.0f;
    cfg.multiplier_cap = 3.0f;
    Score s(cfg);
    auto a = s.add(100, 100);  // would be 101.0, capped to 3.0
    EXPECT_EQ(a, 300);
}

TEST(Score, HighScoreUpdatesOnce) {
    Score s;
    int high_calls = 0;
    std::int64_t last_high = 0;
    s.set_on_high_score([&](std::int64_t h) {
        ++high_calls;
        last_high = h;
    });
    s.add(50);
    EXPECT_EQ(s.high_score(), 50);
    EXPECT_EQ(high_calls, 1);
    s.add(20);
    EXPECT_EQ(s.high_score(), 70);
    EXPECT_EQ(high_calls, 2);
    EXPECT_EQ(last_high, 70);
}

TEST(Score, ResetPreservesHighScore) {
    Score s;
    s.add(100);
    s.add(50);
    EXPECT_EQ(s.score(), 150);
    EXPECT_EQ(s.high_score(), 150);
    s.reset();
    EXPECT_EQ(s.score(), 0);
    EXPECT_EQ(s.high_score(), 150);
    s.add(80);
    EXPECT_EQ(s.score(), 80);
    EXPECT_EQ(s.high_score(), 150);
}

TEST(Score, OnChangeFiresOnEveryAddAndReset) {
    Score s;
    int change_calls = 0;
    s.set_on_change([&](std::int64_t) { ++change_calls; });
    s.add(10);
    s.add(20);
    s.reset();
    EXPECT_EQ(change_calls, 3);
}

TEST(Score, SetHighScoreInitial) {
    Score s;
    int high_calls = 0;
    s.set_on_high_score([&](std::int64_t) { ++high_calls; });
    s.set_high_score(1000);
    EXPECT_EQ(s.high_score(), 1000);
    EXPECT_EQ(high_calls, 0);
    s.add(500);
    EXPECT_EQ(s.high_score(), 1000);  // not beaten
    EXPECT_EQ(high_calls, 0);
    s.add(600);
    EXPECT_EQ(s.high_score(), 1100);
    EXPECT_EQ(high_calls, 1);
}
