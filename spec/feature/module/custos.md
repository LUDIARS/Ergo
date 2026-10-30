# ergo_custos モジュール

LUDIARS の **遠隔テストランナー Custos** がアプリの画面と入力にアクセス
するための in-process HTTP ブリッジ。アプリは本モジュールを link する
だけで、Custos が screenshot を取り、キーイベントを inject できる
ようになる。

## 概要

- カテゴリ: システム
- 公開ヘッダ: `include/ergo/custos/`
- 実装: `src/custos/`
- 依存: `Threads` のみ (Windows では `ws2_32` も)。`ergo_input` には依存しない
  (キーの扱いはアプリが `set_key_handler` で渡す)

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
| `set_key_handler(cb)` | `POST /key` のキーを処理する callback。既定のハンドラは無く、未登録なら 501 |

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

キー入力の既定ハンドラは無い。`ergo_input` を使うアプリは、受け取ったキーを
キーボードデバイスへ inject する glue を自分で登録する (下の「スレッド」の注意どおり、
実際の inject はメインループで行う):

```cpp
ergo::input::KeyboardDevice& kb = *input.keyboard();   // ergo::input::InputSystem
ergo::custos::set_key_handler([&](int c, bool d) {
    kb.injectKeyState(ergo::input::KeyCode(c), d);
});
```

## スレッド

`set_screenshot_provider` / `set_key_handler` の callback は **HTTP の accept
スレッド** で呼ばれる (接続は 1 本ずつ順に処理する)。メインループやレンダー
スレッドの状態を直接触らず、キューなどでメインループへ渡す。

- キー: 上の例のように `injectKeyState` を直接呼ぶのは、入力をメインループと
  同じスレッドで読まないアプリだけにする。通常はキー (`code` / `down`) をロック付きの
  キューに積み、メインループの先頭で `injectKeyState` する
- スクリーンショット: swapchain の読み出しはレンダースレッドでしかできないので、
  `ergo::render::ScreenshotBridge` (`include/ergo/render/screenshot_bridge.h`) を使う。
  provider の中で `request_and_wait(rgba, w, h, timeout_ms)` を呼び、レンダースレッドが
  `consume_request()` → 読み出し → `publish()` で応える

## 設計判断

1. **HTTP / シングル接続** — 多重クライアントや streaming は不要。snapshot
   駆動で十分。WebSocket 化は将来の拡張点。
2. **追加依存ゼロ** — winsock / POSIX socket のみ。PNG は自前の最小エンコーダ
   (`src/custos/png_writer.cpp`、無圧縮の deflate) で書き、外部ライブラリを使わない。
3. **Vulkan 非依存** — モジュール本体は render-API を知らない。callback
   で各アプリが好きな経路でフレームを取る。
4. **localhost のみが既定** — Cernere は通さない。dev box ローカル運用
   前提で、認証も TLS も持たない。LAN に開きたければ `host` を変える。
5. **キーの解釈はアプリ側** — 本モジュールは `code` を解釈せず、`set_key_handler`
   の callback に渡すだけ。`ergo_input` へつなぐ glue はアプリが書く (上の例)。

## 拡張点 (将来)

- WS streaming で連続キャプチャ (ergo_bind と同居も可)
- マウスイベントの inject (現状は key のみ)
- アプリ側 callback ではなく Vulkan layer として hook (今回スコープ外)

## テスト

### Content-Length の変換失敗

関連タスク: `actio:45d56235-e75d-4b58-b8db-0e0c87fb54fe`。
数値に変換できない、または既存の `std::stoul` の範囲外となる
Content-Length は、リクエスト解析失敗として既存の `400 bad request` を返す。
変換例外を受信スレッドへ漏らさず、後続の接続を処理できること。
正常な長さの変換と本文受信は従来どおりとし、本文上限は追加しない。

Augur 計画 (`bug_fix` / `investigate_first`) に対応する回帰ケースは
`tests/custos/test_custos_module.cpp` の `content_length_*` に追加する。
空値・空白のみ・非数値・変換範囲外の 400 と後続接続の成功、および
正常値・前後の空白・先頭ゼロ・ゼロ長の既存動作を確認する。

`tests/custos/test_http_server.cpp` で `HttpResponse` のヘルパ (text / png / json /
404 / 501) を単体で確認する。`tests/custos/test_custos_module.cpp` で実際に起動した
サーバに接続し、`/health`・`/key` の callback・`/screenshot` (PNG / 503)・未知のパスの 404・
上記の Content-Length の扱いを確認する。
