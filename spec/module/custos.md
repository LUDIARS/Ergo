# ergo_custos モジュール

LUDIARS の **遠隔テストランナー Custos** がアプリの画面と入力にアクセス
するための in-process HTTP ブリッジ。アプリは本モジュールを link する
だけで、Custos が screenshot を取り、キーイベントを inject できる
ようになる。

## 概要

- カテゴリ: システム
- 公開ヘッダ: `include/ergo/custos/`
- 実装: `src/custos/`
- 依存: `ergo_input` (キー inject の既定ハンドラ用)

## 必要なデータ

なし。アプリ側 callback で render-API 別のフレーム取得と、key inject を
受ける。

## 公開 API (custos_module.h)

| 関数 | 用途 |
|------|------|
| `start(StartConfig)` | HTTP サーバを別スレッドで起動 |
| `shutdown()` | 停止 (idempotent) |
| `is_running()` / `bound_port()` | 状態問い合わせ |
| `set_screenshot_provider(cb)` | `GET /screenshot` でアプリにフレームを取らせる callback |
| `set_key_handler(cb)` | `POST /key` のキーを処理する callback (default は ergo_input へ inject) |

## HTTP エンドポイント

| メソッド | パス | 用途 |
|---------|------|------|
| `GET`  | `/health`     | `200 ok` (text/plain) |
| `GET`  | `/screenshot` | 最新フレームを PNG で返却。provider 未登録なら 501、未準備なら 503 |
| `POST` | `/key`        | body `{"code": int, "down": bool}` でキーイベントを inject |

## アプリ統合例 (Pictor ocean demo)

```cpp
#include <ergo/custos/custos_module.h>

void on_main_init(VulkanContext& vk_ctx) {
    ergo::custos::start({ .port = 5198 });

    ergo::custos::set_screenshot_provider(
        [&](ergo::custos::ScreenshotData& out) {
            // VulkanContext から最新の swapchain image を staging buffer に
            // 1 回 vkCmdCopyImage → vkQueueSubmit → fence wait。
            // staging を vkMapMemory して RGBA8 を out.rgba に詰める。
            return vk_ctx.read_swapchain_to_rgba(out.rgba, out.width, out.height);
        });
}
```

キー入力はデフォルト (ergo_input への inject) で十分なケースが多い。

## 設計判断

1. **HTTP / シングル接続** — 多重クライアントや streaming は不要。snapshot
   駆動で十分。WebSocket 化は将来の拡張点。
2. **追加依存ゼロ** — winsock / POSIX socket のみ。stb_image_write を
   `third_party/stb/` に vendor して PNG encode に使う。
3. **Vulkan 非依存** — モジュール本体は render-API を知らない。callback
   で各アプリが好きな経路でフレームを取る。
4. **localhost のみが既定** — Cernere は通さない。dev box ローカル運用
   前提で、認証も TLS も持たない。LAN に開きたければ `host` を変える。
5. **キー inject は ergo_input 経由が既定** — `keyboardDevice().injectKeyState`
   をそのまま叩くので、ergo_input を使っているアプリは追加実装不要。

## 拡張点 (将来)

- WS streaming で連続キャプチャ (ergo_bind と同居も可)
- マウスイベントの inject (現状は key のみ)
- アプリ側 callback ではなく Vulkan layer として hook (今回スコープ外)

## テスト

`tests/custos/test_http.cpp` で HTTP request パーサ単体。
`tests/custos/test_module.cpp` で start/shutdown のスモーク。
