#pragma once

// ============================================================
// ergo_custos — Custos remote test runner との IPC ブリッジ。
// ============================================================
//
// アプリ (AdventureCube / Pictor demos など) を Custos の制御画面から
// 遠隔で「キャプチャ + 操作」したいときに、本モジュールを link する。
// 起動するとローカル HTTP サーバを listen し、Custos backend から:
//
//   - GET  /health          ヘルス確認
//   - GET  /screenshot      最新フレームを PNG で返す
//   - POST /key  body=JSON  キーイベントをアプリへ inject
//
// が叩けるようになる。
//
// アプリ統合は最小 3 ステップ:
//   1. `ergo::custos::start({ .port = 5198 })` を main で呼ぶ
//   2. `set_screenshot_provider(...)` で Vulkan swapchain → host RGBA
//      を返す callback を登録
//   3. `set_key_handler(...)` で キーイベントの処理を登録 (ergo_input を
//      使うアプリは `keyboardDevice.injectKeyState(KeyCode(code), down)` を
//      呼ぶだけ)
//
// 設計方針:
//   - **追加依存ゼロ** — winsock2 / POSIX socket のみ、外部 HTTP lib なし
//   - **Vulkan に非依存** — screenshot は callback 経由なので、アプリ側
//     が render API を選べる (Vulkan / WebGPU / 他)
//   - **シングル接続前提** — Custos backend 1 個との 1:1 想定。多重
//     クライアント / WS / streaming は不要 (snapshot 駆動)
//   - **localhost のみが既定** — Cernere 認証は通さない、信頼できる
//     dev box のみで使う前提

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ergo::custos {

// ─── 起動設定 ───────────────────────────────────

struct StartConfig {
    /// HTTP listen ポート。Custos backend の apps.json と一致させる。
    uint16_t    port = 5198;
    /// Bind アドレス。デフォルトは localhost のみ。LAN に開くなら "0.0.0.0"。
    std::string host = "127.0.0.1";
};

/// HTTP サーバを別スレッドで起動する。bind 失敗時 false。
/// 既に起動中なら no-op で true を返す。
bool start(const StartConfig& cfg = {});

/// graceful shutdown。idempotent。
void shutdown();

bool is_running();

uint16_t bound_port();

// ─── スクリーンショット ───────────────────────

/// `GET /screenshot` で返却するピクセルデータ。
/// row-major、top-to-bottom、tightly packed RGBA8。
struct ScreenshotData {
    std::vector<std::uint8_t> rgba;
    std::uint32_t             width  = 0;
    std::uint32_t             height = 0;
};

/// アプリが「現フレームを RGBA でくれ」と問われたときに呼ばれる。
/// 用意できたら out に詰めて true を返す。準備中なら false (HTTP 503)。
using ScreenshotProvider = std::function<bool(ScreenshotData& out)>;

/// 登録。null を渡すと screenshot endpoint は 501 を返すようになる。
void set_screenshot_provider(ScreenshotProvider provider);

// ─── キー入力 ─────────────────────────────────

/// `POST /key { code: <int>, down: <bool> }` を受けたときの処理。
/// `keycode` は ergo::input::KeyCode enum の値 (整数) を期待するが、本
/// モジュール自体は中身を解釈しない。アプリ側 callback が好きに扱う。
using KeyInjectHandler = std::function<void(int keycode, bool down)>;

/// 登録。null を渡すと /key endpoint は 501 を返すようになる。
void set_key_handler(KeyInjectHandler handler);

} // namespace ergo::custos
