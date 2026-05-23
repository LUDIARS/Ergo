#include "gtest/gtest.h"

#include "ergo/render/screenshot_bridge.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace ergo::render;

TEST(ScreenshotBridge, NoPendingRequestInitially) {
    ScreenshotBridge b;
    EXPECT_FALSE(b.has_pending_request());
    // 要求が無いとき consume_request は false。
    EXPECT_FALSE(b.consume_request());
}

TEST(ScreenshotBridge, TimesOutWithoutRenderThread) {
    ScreenshotBridge b;
    std::vector<std::uint8_t> rgba;
    uint32_t w = 0, h = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = b.request_and_wait(rgba, w, h, 50);
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    EXPECT_FALSE(ok);
    // タイムアウト分は待っている。
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                  .count(),
              40);
}

TEST(ScreenshotBridge, RenderThreadFulfillsRequest) {
    ScreenshotBridge b;
    std::atomic<bool> stop{false};

    // レンダースレッド役: 保留要求があれば固定バイト列で publish する。
    std::thread render([&] {
        while (!stop.load()) {
            if (b.consume_request()) {
                std::vector<std::uint8_t> px = {1, 2, 3, 4, 5, 6, 7, 8};
                b.publish(std::move(px), 2, 1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    std::vector<std::uint8_t> rgba;
    uint32_t w = 0, h = 0;
    const bool ok = b.request_and_wait(rgba, w, h, 2000);
    stop.store(true);
    render.join();

    EXPECT_TRUE(ok);
    EXPECT_EQ(w, 2u);
    EXPECT_EQ(h, 1u);
    ASSERT_EQ(rgba.size(), 8u);
    EXPECT_EQ(rgba[0], 1u);
    EXPECT_EQ(rgba[7], 8u);
}

TEST(ScreenshotBridge, PublishFailureUnblocksWaiter) {
    ScreenshotBridge b;
    std::atomic<bool> stop{false};

    std::thread render([&] {
        while (!stop.load()) {
            if (b.consume_request()) {
                b.publish_failure();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    std::vector<std::uint8_t> rgba;
    uint32_t w = 0, h = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = b.request_and_wait(rgba, w, h, 2000);
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    stop.store(true);
    render.join();

    // 失敗通知で待機は即解ける (タイムアウト 2000ms を待たない)。
    EXPECT_FALSE(ok);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                  .count(),
              1500);
}

TEST(ScreenshotBridge, ConsumeRequestIsOneShot) {
    ScreenshotBridge b;
    std::vector<std::uint8_t> rgba;
    uint32_t w = 0, h = 0;

    // 別スレッドで要求を立て、 メインスレッドで consume する。
    std::thread waiter([&] { b.request_and_wait(rgba, w, h, 500); });

    // 要求が立つまで待つ。
    while (!b.has_pending_request()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // 1 回目の consume は true、 2 回目は false (one-shot)。
    EXPECT_TRUE(b.consume_request());
    EXPECT_FALSE(b.consume_request());

    // waiter をタイムアウトで終わらせる。
    waiter.join();
}
