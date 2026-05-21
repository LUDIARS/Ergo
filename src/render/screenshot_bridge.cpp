#include "ergo/render/screenshot_bridge.h"

#include <chrono>

namespace ergo::render {

bool ScreenshotBridge::request_and_wait(std::vector<std::uint8_t>& out_rgba,
                                        uint32_t& out_w, uint32_t& out_h,
                                        int timeout_ms) {
    // KuzuSurvivors の WorldRenderer::screenshot_blocking 相当。
    // 「要求前の serial」を控え、 待機条件で serial が進んだことを確認する
    // ことで、 古い publish 結果を取り違えないようにする。
    const uint64_t last = serial_.load(std::memory_order_acquire);

    std::unique_lock<std::mutex> lk(shot_mu_);
    ready_     = false;
    failed_    = false;
    requested_.store(true, std::memory_order_release);

    const bool got = shot_cv_.wait_for(
        lk, std::chrono::milliseconds(timeout_ms),
        [&] {
            // 成功 (ready_) でも失敗 (failed_) でも待機を解く。 ただし
            // この request_and_wait 呼び出しに対応する新しい serial の
            // publish/publish_failure であること。
            return (ready_ || failed_) &&
                   serial_.load(std::memory_order_acquire) > last;
        });

    if (!got || failed_) {
        // タイムアウト、 または レンダースレッドが失敗を通知した。
        // 取りこぼした要求が残らないよう要求フラグを下ろす。
        requested_.store(false, std::memory_order_release);
        return false;
    }

    out_rgba = std::move(rgba_);
    out_w    = w_;
    out_h    = h_;
    return true;
}

bool ScreenshotBridge::consume_request() {
    // 要求が立っていれば true を返しつつ、 同時にフラグを下ろす
    // (exchange による 1 回限りの消費)。
    return requested_.exchange(false, std::memory_order_acquire);
}

void ScreenshotBridge::publish(std::vector<std::uint8_t>&& rgba,
                               uint32_t w, uint32_t h) {
    {
        std::lock_guard<std::mutex> lk(shot_mu_);
        rgba_   = std::move(rgba);
        w_      = w;
        h_      = h;
        ready_  = true;
        failed_ = false;
        serial_.fetch_add(1, std::memory_order_release);
    }
    shot_cv_.notify_all();
}

void ScreenshotBridge::publish_failure() {
    {
        std::lock_guard<std::mutex> lk(shot_mu_);
        ready_  = false;
        failed_ = true;
        serial_.fetch_add(1, std::memory_order_release);
    }
    shot_cv_.notify_all();
}

} // namespace ergo::render
