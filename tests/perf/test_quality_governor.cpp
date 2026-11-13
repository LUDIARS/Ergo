#include "gtest/gtest.h"

#include <vector>

#include "ergo/perf/quality_governor.h"
#include "ergo/perf/quality_ladder.h"

using namespace ergo::perf;

namespace {

/// A renderer's stages: the fog may lighten to level 3 but never goes (its
/// lightest level still draws it), SSAO to 2, bloom to 1.
std::vector<StageDecl> stages() { return {{"fog", 3}, {"ssao", 2}, {"bloom", 1}}; }

GovernorConfig quick() {
    GovernorConfig c;
    c.target_ms = 16.0;
    c.window = 10;
    c.degrade_hold = 5;
    c.upgrade_hold = 30;
    c.settle = 3;
    return c;
}

/// Feeds `frames` frames of `ms`; returns how many lighter / heavier steps were taken.
struct Moves {
    int lighter = 0;
    int heavier = 0;
};
Moves feed(QualityGovernor& g, double ms, int frames) {
    Moves m;
    for (int i = 0; i < frames; ++i) {
        const GovernorChange c = g.observe(ms);
        if (c == GovernorChange::lighter) ++m.lighter;
        if (c == GovernorChange::heavier) ++m.heavier;
    }
    return m;
}

} // namespace

TEST(QualityLadder, DefaultOrderLightensEveryStageALevelAtATime) {
    const QualityLadder ladder(stages());
    EXPECT_EQ(ladder.step_count(), 6);  // 3 + 2 + 1
    EXPECT_EQ(ladder.levels_at(0), (std::vector<int>{0, 0, 0}));
    EXPECT_EQ(ladder.levels_at(1), (std::vector<int>{1, 0, 0}));
    EXPECT_EQ(ladder.levels_at(3), (std::vector<int>{1, 1, 1}));
    EXPECT_EQ(ladder.levels_at(5), (std::vector<int>{2, 2, 1}));
    EXPECT_EQ(ladder.levels_at(6), (std::vector<int>{3, 2, 1}));
    EXPECT_EQ(ladder.levels_at(99), (std::vector<int>{3, 2, 1}));  // never past the lightest declared
}

TEST(QualityLadder, CallerWrittenOrderIsChecked) {
    const auto ok = QualityLadder::from_steps(stages(), {{1, 1}, {0, 1}, {0, 2}, {2, 1}});
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok->step_count(), 4);
    EXPECT_EQ(ok->levels_at(3), (std::vector<int>{2, 1, 0}));
    EXPECT_FALSE(QualityLadder::from_steps(stages(), {{0, 2}}).has_value());          // skips level 1
    EXPECT_FALSE(QualityLadder::from_steps(stages(), {{2, 1}, {2, 2}}).has_value());  // past bloom's lightest
    EXPECT_FALSE(QualityLadder::from_steps(stages(), {{7, 1}}).has_value());          // no such stage
}

TEST(QualityLadder, FindsStagesByName) {
    const QualityLadder ladder(stages());
    EXPECT_EQ(ladder.find("ssao").value(), 1u);
    EXPECT_FALSE(ladder.find("shadows").has_value());
}

TEST(QualityGovernor, StaysPutWithinBudget) {
    QualityGovernor g(QualityLadder(stages()), quick());
    const Moves m = feed(g, 14.0, 500);  // under the budget, above the upgrade line
    EXPECT_EQ(m.lighter + m.heavier, 0);
    EXPECT_EQ(g.step(), 0);
}

TEST(QualityGovernor, LightensUnderSustainedLoadButNeverPastTheFloor) {
    QualityGovernor g(QualityLadder(stages()), quick());
    feed(g, 40.0, 5000);  // far over budget whatever the step
    EXPECT_EQ(g.step(), g.step_count());
    EXPECT_EQ(g.level("fog"), 3);  // at its declared lightest, still drawn
    EXPECT_EQ(g.level("bloom"), 1);
    EXPECT_EQ(g.level("nope"), -1);
}

TEST(QualityGovernor, LoneSpikesDoNotMoveIt) {
    QualityGovernor g(QualityLadder(stages()), quick());
    int lighter = 0;
    for (int i = 0; i < 2000; ++i) {
        // A 100 ms hitch every 25 frames: under 10% of any 10-frame window.
        if (g.observe(i % 25 == 0 ? 100.0 : 14.0) == GovernorChange::lighter) ++lighter;
    }
    EXPECT_EQ(lighter, 0);
    EXPECT_EQ(g.step(), 0);
}

TEST(QualityGovernor, OneStepPerDecisionAndSettlesBetween) {
    QualityGovernor g(QualityLadder(stages()), quick());
    // settle 3, then the window fills (10, its last frame is the first judged) and
    // stays over for hold 5: the step comes on frame 3 + 10 + 5 - 1, then the same again.
    const Moves first = feed(g, 20.0, 3 + 10 + 5 - 2);
    EXPECT_EQ(first.lighter, 0);
    EXPECT_EQ(g.observe(20.0), GovernorChange::lighter);
    EXPECT_EQ(g.step(), 1);
    const Moves next = feed(g, 20.0, 3 + 10 + 5 - 2);
    EXPECT_EQ(next.lighter, 0);  // the window refills at the new step before judging
    EXPECT_EQ(g.observe(20.0), GovernorChange::lighter);
    EXPECT_EQ(g.step(), 2);
}

TEST(QualityGovernor, ComesBackWithHeadroomAndBacksOffFlipFlops) {
    QualityGovernor g(QualityLadder(stages()), quick());
    g.set_step(2);
    // Plenty of headroom: steps back toward full quality.
    const Moves up = feed(g, 8.0, 3 + 10 + 30);
    EXPECT_EQ(up.heavier, 1);
    EXPECT_EQ(g.step(), 1);
    // That step was too heavy: it is taken back, and the next upgrade waits twice as long.
    const Moves down = feed(g, 20.0, 3 + 10 + 5);
    EXPECT_EQ(down.lighter, 1);
    EXPECT_EQ(g.step(), 2);
    EXPECT_EQ(g.upgrade_hold(), 60u);
}

TEST(QualityGovernor, PinnedHoldsTheStep) {
    QualityGovernor g(QualityLadder(stages()), quick());
    g.pin(4);
    feed(g, 40.0, 1000);
    feed(g, 5.0, 1000);
    EXPECT_EQ(g.step(), 4);
    EXPECT_TRUE(g.pinned());
    g.unpin();
    feed(g, 40.0, 3 + 10 + 5);
    EXPECT_EQ(g.step(), 5);
}

TEST(QualityGovernor, IgnoresUnmeasuredFrames) {
    QualityGovernor g(QualityLadder(stages()), quick());
    feed(g, 0.0, 1000);
    feed(g, -3.0, 1000);
    EXPECT_EQ(g.step(), 0);
    EXPECT_DOUBLE_EQ(g.statistic(), 0.0);
}
