#include "gtest/gtest.h"

#include <chrono>
#include <thread>

#include "ergo/frame/counter.h"
#include "ergo/frame/frame.h"

using namespace ergo::frame;

namespace {

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace

TEST(FrameCounter, StartsAtZero) {
    Counter::instance().reset();
    EXPECT_EQ(Counter::instance().count(), 0u);
    EXPECT_FLOAT_EQ(Counter::instance().fps(),        0.0f);
    EXPECT_FLOAT_EQ(Counter::instance().dt_seconds(), 0.0f);
}

TEST(FrameCounter, TickIncrementsCount) {
    Counter::instance().reset();
    Counter::instance().tick();
    Counter::instance().tick();
    Counter::instance().tick();
    EXPECT_EQ(Counter::instance().count(), 3u);
}

TEST(FrameCounter, FirstTickHasZeroDt) {
    Counter::instance().reset();
    Counter::instance().tick();
    // No prior frame -> dt is 0, FPS still 0.
    EXPECT_FLOAT_EQ(Counter::instance().dt_seconds(), 0.0f);
    EXPECT_FLOAT_EQ(Counter::instance().fps(),        0.0f);
}

TEST(FrameCounter, FpsApproximatesTickRate) {
    Counter::instance().reset();
    Counter::instance().set_window_size(10);
    Counter::instance().tick();
    // 10 more ticks, ~10 ms apart -> ~100 FPS.
    for (int i = 0; i < 10; ++i) {
        sleep_ms(10);
        Counter::instance().tick();
    }
    const float f = Counter::instance().fps();
    // Loose bounds — sleep precision varies across platforms.
    EXPECT_GT(f, 30.0f);
    EXPECT_LT(f, 300.0f);
}

TEST(FrameCounter, ResetClearsState) {
    Counter::instance().reset();
    for (int i = 0; i < 5; ++i) Counter::instance().tick();
    EXPECT_GT(Counter::instance().count(), 0u);
    Counter::instance().reset();
    EXPECT_EQ   (Counter::instance().count(), 0u);
    EXPECT_FLOAT_EQ(Counter::instance().fps(), 0.0f);
}

TEST(FrameCounter, WindowSizeMinimumOne) {
    Counter::instance().reset();
    Counter::instance().set_window_size(0);
    EXPECT_EQ(Counter::instance().window_size(), 1u);
    Counter::instance().set_window_size(3);
    EXPECT_EQ(Counter::instance().window_size(), 3u);
}

TEST(FrameCounter, FormatHudIsNonEmpty) {
    Counter::instance().reset();
    for (int i = 0; i < 3; ++i) Counter::instance().tick();
    const std::string hud = format_hud();
    EXPECT_FALSE(hud.empty());
    // Prefix sanity — expect "F" + digits.
    EXPECT_EQ(hud[0], 'F');
}
