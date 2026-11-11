// The lesson sample's simulation: the observations report what really
// happened (through the real ergo_world_time engine), so they stay 0 while
// an effect is off and turn > 0 after the next hit once it is on.
#include "action_feel_sim.h"

#include <gtest/gtest.h>

using namespace lesson_action_feel;

namespace {

constexpr float kDt = 1.0f / 60.0f;

ergo::world_time::Engine& fresh_world() {
    auto& w = ergo::world_time::Engine::instance();
    w.force_stop();
    return w;
}

/// Frames until the next hit (inclusive); -1 when none within `limit`.
int frames_to_next_hit(ActionFeelSim& sim, const FeelToggles& t, ergo::world_time::Engine& w, int limit = 600) {
    for (int i = 1; i <= limit; ++i) {
        if (sim.frame(kDt, t, w).hit) return i;
    }
    return -1;
}

} // namespace

TEST(LessonActionFeelSim, PlainRunHitsButAppliesNoEffect) {
    auto&         w = fresh_world();
    ActionFeelSim sim;
    const FeelToggles plain{false, false, false};
    for (int i = 0; i < 300; ++i) {
        const auto f = sim.frame(kDt, plain, w);
        EXPECT_FALSE(w.is_hit_stop());
        EXPECT_TRUE(f.world_dt == kDt);   // never frozen
    }
    // 5 s of world time, one hit every 0.5 s from t = 0: 10, or 11 when float rounding lands the last one on frame 300
    EXPECT_GE(sim.hits(), 10L);
    EXPECT_LE(sim.hits(), 11L);
    EXPECT_TRUE(sim.observed().hit_stop_last_seconds == 0.0);
    EXPECT_TRUE(sim.observed().knockback_last_impulse == 0.0);
    EXPECT_TRUE(sim.enemy_offset() == 0.0f);
    EXPECT_TRUE(sim.shake() == 0.0f);
}

TEST(LessonActionFeelSim, TurningHitStopOnShowsUpAfterTheNextHit) {
    auto&         w = fresh_world();
    ActionFeelSim sim;
    FeelToggles   t{false, false, false};
    ASSERT_EQ(frames_to_next_hit(sim, t, w), 1);   // the first hit lands at once
    EXPECT_TRUE(sim.observed().hit_stop_last_seconds == 0.0);

    t.hit_stop = true;   // like a set from the Studio between two frames
    EXPECT_TRUE(sim.observed().hit_stop_last_seconds == 0.0);   // nothing happened yet
    ASSERT_TRUE(frames_to_next_hit(sim, t, w) > 0);
    EXPECT_TRUE(w.is_hit_stop());                   // the real world-time engine froze
    EXPECT_GT(sim.observed().hit_stop_last_seconds, 0.07);
    EXPECT_LT(sim.observed().hit_stop_last_seconds, 0.09);

    // The freeze is real: world time stands still for the 0.08 s (5 frames at 60 Hz), then runs again.
    int frozen = 0;
    while (sim.frame(kDt, t, w).world_dt == 0.0f) ++frozen;
    EXPECT_GE(frozen, 4);
    EXPECT_LE(frozen, 5);
    EXPECT_TRUE(sim.observed().knockback_last_impulse == 0.0);   // still off
}

TEST(LessonActionFeelSim, KnockbackAndShakeFollowTheirToggles) {
    auto&         w = fresh_world();
    ActionFeelSim sim;
    FeelToggles   t{false, true, true};
    ASSERT_EQ(frames_to_next_hit(sim, t, w), 1);
    EXPECT_TRUE(sim.observed().knockback_last_impulse == 6.0);
    EXPECT_TRUE(sim.shake() > 0.29f);
    for (int i = 0; i < 6; ++i) sim.frame(kDt, t, w);
    EXPECT_GT(sim.enemy_offset(), 0.0f);            // pushed back
    EXPECT_TRUE(sim.observed().hit_stop_last_seconds == 0.0);

    t = FeelToggles{false, false, false};           // turning it off keeps the last observation (it is "last")
    for (int i = 0; i < 120; ++i) sim.frame(kDt, t, w);
    EXPECT_TRUE(sim.observed().knockback_last_impulse == 6.0);
    EXPECT_LT(sim.shake(), 0.01f);
}

TEST(LessonActionFeelSim, SameInputsSameRun) {
    auto&         w = fresh_world();
    ActionFeelSim a, b;
    const FeelToggles full;
    for (int i = 0; i < 200; ++i) a.frame(kDt, full, w);
    w.force_stop();
    for (int i = 0; i < 200; ++i) b.frame(kDt, full, w);
    EXPECT_EQ(a.hits(), b.hits());
    EXPECT_TRUE(a.enemy_offset() == b.enemy_offset());
    EXPECT_TRUE(a.world_seconds() == b.world_seconds());
}
