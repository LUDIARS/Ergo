#include "ergo/timing_judge/timing_judge.h"

#include "gtest/gtest.h"

#include <string>

using ergo::timing_judge::breaks_combo;
using ergo::timing_judge::judge;
using ergo::timing_judge::Judgment;
using ergo::timing_judge::name;
using ergo::timing_judge::Windows;

TEST(TimingJudge, ExactPerfect) {
    Windows w{25, 60, 120};
    EXPECT_EQ(judge(1000, 1000, w), Judgment::Perfect);
}

TEST(TimingJudge, BoundaryInclusive) {
    Windows w{25, 60, 120};
    EXPECT_EQ(judge(1000, 1025, w), Judgment::Perfect);
    EXPECT_EQ(judge(1000, 1026, w), Judgment::Great);
    EXPECT_EQ(judge(1000, 1060, w), Judgment::Great);
    EXPECT_EQ(judge(1000, 1061, w), Judgment::Good);
    EXPECT_EQ(judge(1000, 1120, w), Judgment::Good);
    EXPECT_EQ(judge(1000, 1121, w), Judgment::Miss);
}

TEST(TimingJudge, NegativeDeltaSymmetric) {
    Windows w{25, 60, 120};
    EXPECT_EQ(judge(1000, 980, w), Judgment::Perfect);   // -20
    EXPECT_EQ(judge(1000, 970, w), Judgment::Great);     // -30
    EXPECT_EQ(judge(1000, 900, w), Judgment::Good);      // -100
    EXPECT_EQ(judge(1000, 800, w), Judgment::Miss);      // -200
}

TEST(TimingJudge, NameLookup) {
    EXPECT_EQ(std::string(name(Judgment::Perfect)), std::string("PERFECT"));
    EXPECT_EQ(std::string(name(Judgment::Great)),   std::string("GREAT"));
    EXPECT_EQ(std::string(name(Judgment::Good)),    std::string("GOOD"));
    EXPECT_EQ(std::string(name(Judgment::Miss)),    std::string("MISS"));
}

TEST(TimingJudge, BreaksCombo) {
    // min_kept = Good means only Miss breaks
    EXPECT_FALSE(breaks_combo(Judgment::Perfect, Judgment::Good));
    EXPECT_FALSE(breaks_combo(Judgment::Great,   Judgment::Good));
    EXPECT_FALSE(breaks_combo(Judgment::Good,    Judgment::Good));
    EXPECT_TRUE (breaks_combo(Judgment::Miss,    Judgment::Good));

    // Stricter: min_kept = Great means Good and Miss break
    EXPECT_FALSE(breaks_combo(Judgment::Perfect, Judgment::Great));
    EXPECT_FALSE(breaks_combo(Judgment::Great,   Judgment::Great));
    EXPECT_TRUE (breaks_combo(Judgment::Good,    Judgment::Great));
    EXPECT_TRUE (breaks_combo(Judgment::Miss,    Judgment::Great));
}
