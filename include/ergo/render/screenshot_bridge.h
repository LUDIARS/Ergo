#pragma once

/// ergo_render — スクリーンショットの thread-bridge `ScreenshotBridge`。
///
/// Custos などの遠隔テストランナーは HTTP スレッドから「今のフレームを 1 枚
/// 撮れ」と要求するが、 swapchain image の読み出しはレンダースレッドでしか
/// できない。 ScreenshotBridge は その HTTP スレッド ↔ レンダースレッド間の
/// 受け渡しを mutex / condition_variable / serial 番号で同期する。
///
/// ゲームの WorldRenderer が個別に持っていた `shot_mu_` / `shot_cv_` /
/// `shot_requested_` / `shot_serial_` 一式をそのまま共通化したもの。
///
/// 使い方:
///   HTTP スレッド側:
///     std::vector<uint8_t> rgba; uint32_t w, h;
///     if (bridge.request_and_wait(rgba, w, h, 2000)) { ... }
///
///   レンダースレッド側 (run_frame の末尾など):
///     if (bridge.consume_request()) {
///         // ここで swapchain image を読み出して rgba/w/h を埋める
///         bridge.publish(rgba, w, h);   // 失敗時は publish_failure()
///     }
///
/// ScreenshotBridge 自体は Vulkan に触れない — 実際のピクセル読み出しは
/// 呼び出し側 (ゲーム / Pictor) が行い、 結果バイト列だけをここに渡す。

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace ergo::render {

/// HTTP スレッドとレンダースレッドの間でスクリーンショットを受け渡す同期器。
class ScreenshotBridge {
public:
    ScreenshotBridge() = default;
    ~ScreenshotBridge() = default;

    ScreenshotBridge(const ScreenshotBridge&)            = delete;
    ScreenshotBridge& operator=(const ScreenshotBridge&) = delete;

    /// [任意スレッド] スクリーンショットを要求し、 完了まで待つ。
    /// レンダースレッドが `consume_request()` → `publish()` を済ませたら
    /// `out_rgba` / `out_w` / `out_h` に結果を移して true を返す。
    /// `timeout_ms` 以内に結果が来なければ false。
    bool request_and_wait(std::vector<std::uint8_t>& out_rgba,
                          uint32_t& out_w, uint32_t& out_h,
                          int timeout_ms = 2000);

    /// [レンダースレッド] 保留中の要求があれば消費して true を返す。
    /// true のときだけ呼び出し側はキャプチャを行い `publish()` する。
    /// 要求が無ければ false (この場合キャプチャ不要)。
    bool consume_request();

    /// [レンダースレッド] キャプチャ成功。 結果バイト列を待機側へ引き渡す。
    /// `rgba` は move される。
    void publish(std::vector<std::uint8_t>&& rgba, uint32_t w, uint32_t h);

    /// [レンダースレッド] キャプチャ失敗を待機側へ通知する。
    /// `request_and_wait` はタイムアウトまで待たずに即 false を返せる。
    void publish_failure();

    /// 現在 保留中の要求があるか (主にテスト/診断用)。
    bool has_pending_request() const {
        return requested_.load(std::memory_order_acquire);
    }

private:
    std::mutex                shot_mu_;
    std::condition_variable   shot_cv_;
    std::atomic<bool>         requested_{false};
    std::atomic<uint64_t>     serial_{0};
    bool                      ready_  = false;
    bool                      failed_ = false;
    std::vector<std::uint8_t> rgba_;
    uint32_t                  w_ = 0;
    uint32_t                  h_ = 0;
};

} // namespace ergo::render
