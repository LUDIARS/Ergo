#include "ergo/world_time/world_time.h"

#include "gtest/gtest.h"

#include <cmath>

using ergo::world_time::Engine;
using ergo::world_time::Ease;
using ergo::world_time::Phase;
using ergo::world_time::ITimeScaleTarget;

namespace {

class CapturingTarget : public ITimeScaleTarget {
public:
    void on_time_scale_update(float ts) override {
        last_scale = ts;
        ++calls;
    }
    float last_scale = -1.0f;
    int   calls      = 0;
};

class WorldTimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Singleton state can leak across tests; reset.
        Engine::instance().force_stop();
    }
    void TearDown() override {
        Engine::instance().force_stop();
    }
};

constexpr float EPS = 1e-5f;

bool near_eq(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

// --- idle / no effect --------------------------------------------------------

TEST_F(WorldTimeTest, idle_returns_dt_unchanged) {
    auto& e = Engine::instance();
    EXPECT_EQ(e.current_phase(), Phase::None);
    EXPECT_TRUE(near_eq(e.current_time_scale(), 1.0f));
    EXPECT_TRUE(near_eq(e.update(0.016f), 0.016f));
}

// --- HitStop -----------------------------------------------------------------

TEST_F(WorldTimeTest, hit_stop_freezes_then_releases) {
    auto& e = Engine::instance();
    e.hit_stop(0.1f);
    EXPECT_TRUE(e.is_hit_stop());

    EXPECT_TRUE(near_eq(e.update(0.05f), 0.0f));
    EXPECT_TRUE(near_eq(e.current_time_scale(), 0.0f));
    EXPECT_TRUE(e.is_hit_stop());

    // remaining was 0.05; decrement by 0.06 → expires this frame.
    // Semantics: the expiring frame is the release frame (returns full dt).
    EXPECT_TRUE(near_eq(e.update(0.06f), 0.06f));
    EXPECT_FALSE(e.is_hit_stop());
    EXPECT_EQ(e.current_phase(), Phase::None);

    // Subsequent frames pass through normally.
    EXPECT_TRUE(near_eq(e.update(0.02f), 0.02f));
}

TEST_F(WorldTimeTest, new_hit_stop_overrides_previous) {
    auto& e = Engine::instance();
    e.hit_stop(1.0f);   // long
    e.update(0.5f);     // half consumed
    e.hit_stop(0.05f);  // override with short
    e.update(0.04f);
    EXPECT_TRUE(e.is_hit_stop());
    e.update(0.02f);   // 0.05 - 0.04 - 0.02 < 0 -> end
    EXPECT_FALSE(e.is_hit_stop());
}

TEST_F(WorldTimeTest, hit_stop_zero_duration_is_noop) {
    auto& e = Engine::instance();
    e.hit_stop(0.0f);
    EXPECT_FALSE(e.is_hit_stop());
    EXPECT_EQ(e.current_phase(), Phase::None);
}

// --- HitSlow phases ----------------------------------------------------------

TEST_F(WorldTimeTest, hit_slow_three_phase_curve_linear) {
    auto& e = Engine::instance();
    // duration 1.0, hold 0.4, transition = 0.3 each.
    // Linear ease so we can predict scale at sample points.
    e.hit_slow(/*duration*/ 1.0f,
               /*center_weight*/ 0.0f,
               /*center_time_scale*/ 0.2f,
               /*center_hold_time*/ 0.4f,
               Ease::Linear);

    // t=0.15 (mid In): lerp(1.0, 0.2, 0.5) = 0.6
    e.update(0.15f);
    EXPECT_EQ(e.current_phase(), Phase::HitSlowIn);
    EXPECT_TRUE(near_eq(e.current_time_scale(), 0.6f, 1e-3f));

    // t=0.45 (mid Loop): 0.2
    e.update(0.30f);
    EXPECT_EQ(e.current_phase(), Phase::HitSlowLoop);
    EXPECT_TRUE(near_eq(e.current_time_scale(), 0.2f, 1e-3f));

    // t=0.85 (mid Out): lerp(0.2, 1.0, 0.5) = 0.6
    e.update(0.40f);
    EXPECT_EQ(e.current_phase(), Phase::HitSlowOut);
    EXPECT_TRUE(near_eq(e.current_time_scale(), 0.6f, 1e-3f));

    // After total duration -> back to None / 1.0.
    e.update(0.20f);
    EXPECT_EQ(e.current_phase(), Phase::None);
    EXPECT_TRUE(near_eq(e.current_time_scale(), 1.0f));
}

TEST_F(WorldTimeTest, hit_slow_pure_hold_when_hold_equals_duration) {
    auto& e = Engine::instance();
    e.hit_slow(0.2f, 0.0f, 0.5f, 0.2f, Ease::Linear);
    e.update(0.05f);
    EXPECT_EQ(e.current_phase(), Phase::HitSlowLoop);
    EXPECT_TRUE(near_eq(e.current_time_scale(), 0.5f));
}

// --- force_stop --------------------------------------------------------------

TEST_F(WorldTimeTest, force_stop_clears_immediately) {
    auto& e = Engine::instance();
    CapturingTarget t;
    e.register_target(&t);

    e.hit_stop(1.0f);
    e.update(0.1f);
    EXPECT_TRUE(near_eq(t.last_scale, 0.0f));

    e.force_stop();
    EXPECT_EQ(e.current_phase(), Phase::None);
    EXPECT_TRUE(near_eq(e.current_time_scale(), 1.0f));
    EXPECT_TRUE(near_eq(t.last_scale, 1.0f));   // notified once with 1.0

    e.unregister_target(&t);
}

// --- observer ----------------------------------------------------------------

TEST_F(WorldTimeTest, observer_receives_scale_each_update) {
    auto& e = Engine::instance();
    CapturingTarget t;
    e.register_target(&t);

    e.hit_stop(0.05f);
    e.update(0.01f);
    EXPECT_TRUE(near_eq(t.last_scale, 0.0f));
    EXPECT_EQ(t.calls, 1);

    e.update(0.05f);    // ends stop
    EXPECT_TRUE(near_eq(t.last_scale, 1.0f));
    EXPECT_EQ(t.calls, 2);

    e.unregister_target(&t);
}

TEST_F(WorldTimeTest, register_dedup) {
    auto& e = Engine::instance();
    CapturingTarget t;
    e.register_target(&t);
    e.register_target(&t);
    e.register_target(&t);
    EXPECT_EQ(e.target_count(), 1u);
    e.unregister_target(&t);
}

TEST_F(WorldTimeTest, unregister_unknown_is_noop) {
    auto& e = Engine::instance();
    CapturingTarget t;
    e.unregister_target(&t);   // never registered
    EXPECT_EQ(e.target_count(), 0u);
}

TEST_F(WorldTimeTest, null_target_register_is_ignored) {
    auto& e = Engine::instance();
    e.register_target(nullptr);
    EXPECT_EQ(e.target_count(), 0u);
}
