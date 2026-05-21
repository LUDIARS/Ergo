# ergo_render 仕様

## 概要

`ergo_render` は **Pictor の上・ゲームの下** に入る横断オーケストレーション層。

LUDIARS の C++ ゲーム (KuzuSurvivors / AdventureCube) は、 ゲーム World を
Pictor (低レベル Vulkan) に橋渡しする「WorldRenderer」相当を各自手書きし、
横断オーケストレーション — 初期化順序・パス構成・フレームループ・
submit/present・screenshot・破棄順序 — を 1 箇所に溜め込んで God Class 化して
いた。`ergo_render` はこの **横断層だけ** を引き取る共通モジュール。

各ゲームのサブレンダラ (描画プリミティブ) は既に分離できているので、
`ergo_render` は「横断オーケストレーション」だけを担う。 ゲーム固有の
サブレンダラ実装本体・actor→drawable 変換・FrameComposer のパス構成の
組み立ては `ergo_render` に入れず **ゲーム側に残す**。

依存の向きは **ergo_render → Pictor の一方向**。`ergo_render` はどのゲーム
(KS / AC) にも依存しないフレームワークであり、 上位レイヤー (Ergo の他
モジュール / ergo_custos / ホストアプリ) も demo に組み込まない。

## カテゴリ

システム (描画オーケストレーション)

## 所属ドメイン

描画 / フレームループ / Pictor 統合

## 構成要素

### (A) RenderContext — Pictor ハンドル束

サブレンダラ初期化時に渡す Pictor のハンドル一式。 どのハンドルも
**所有しない** (借用のみ)。

| メンバ | 型 | 必須 | 役割 |
|---|---|---|---|
| `vk`         | `pictor::VulkanContext*`      | 必須 | 生 Vulkan の基盤 |
| `surface`    | `pictor::GlfwSurfaceProvider*`| 任意 | ウィンドウ/サーフェス |
| `renderer`   | `pictor::PictorRenderer*`     | 任意 | 高レベルレンダラ (パススルー) |
| `anim`       | `pictor::AnimationSystem*`    | 任意 | skinning 行列ソース |
| `shader_dir` | `std::string`                 | —    | 解決済シェーダディレクトリ |
| `asset_root` | `std::string`                 | —    | 解決済アセットルート |

### (B) IRenderLayer — サブレンダラ統一インターフェース

ゲーム固有の描画レイヤーが実装する純粋仮想インターフェース。 FrameComposer は
レイヤー実体を知らず、 このインターフェース越しにライフサイクルを回す。

```cpp
class IRenderLayer {
public:
  virtual ~IRenderLayer() = default;
  virtual void initialize(RenderContext&) = 0;
  virtual void set_render_pass(VkRenderPass) = 0;
  virtual void on_first_frame(RenderContext&) {}   // 遅延 bake 等
  virtual void update(const FrameContext&) {}      // drawlist 構築等
  virtual void record(VkCommandBuffer, VkExtent2D) = 0;
  virtual void shutdown() = 0;
};
```

### (C) FrameComposer — パス構成 + フレームループ

`add_pass(VkRenderPass, std::vector<IRenderLayer*>)` で「パス列」を組み、
`run_frame(const FrameContext&)` が 1 フレームを回す:

1. `acquire_next_image()` — swapchain image 取得 (fence 待ちは VulkanContext 内部)
2. 初回フレームなら全レイヤー `on_first_frame()`
3. 全レイヤー `update(frame)`
4. コマンドバッファ記録 — パス毎に begin / 各レイヤー record / end
5. `vkQueueSubmit` + `present`

初期化順は add_pass 登録順 × パス内レイヤー順、 破棄順はその逆順。

**パス列の構成違いで描画構成を表現する** (設計意図):

- KS の「postprocess 有効/無効 2 経路」 — 有効なら HDR scene パス + post パス
  + HUD パスの 3 列、 無効なら swapchain default 1 列、 と add_pass の呼び方を
  変えるだけで切り替わる。
- AC は最小構成 — default render pass 1 列に 1 レイヤーを載せるだけ。

framebuffer は `set_framebuffer_provider()` でパス毎に解決関数を登録できる
(post-process の HDR scene パスのような image index 非依存の固定 framebuffer
にも対応)。 未登録のパスは VulkanContext の default framebuffers を使う。

### (D) 横断インフラ

- **ScreenshotBridge** — HTTP スレッド ↔ レンダースレッドのスクリーンショット
  同期。 `mutex` / `condition_variable` / serial 番号で受け渡す。 HTTP 側は
  `request_and_wait()`、 レンダー側は `consume_request()` → `publish()` /
  `publish_failure()`。 ピクセル読み出し自体は呼び出し側 (ゲーム/Pictor) が
  行い、 結果バイト列だけを bridge に渡す。
- **resolve_shader_dir() / resolve_asset_root()** — 実行ファイル近傍を上方探索
  するアセットルート解決ユーティリティ。 解決順は (1) 環境変数
  (`ERGO_RENDER_SHADER_DIR` / `ERGO_RENDER_ASSET_ROOT`) → (2) 起点からの
  上方探索 → (3) フォールバック (起点直下)。

### (E) FrameContext — 毎フレーム可変データ + カメラ math

毎フレーム値が変わるデータ struct: `float dt` / `VkExtent2D extent` /
camera `view[16]`・`proj[16]` 行列 / `uint64_t frame_index`。

カメラ math もこのモジュールに同梱:

- `deg_to_rad()` / `mat4_identity()`
- `look_at_rh()` — 右手系 look-at ビュー行列 (column-major)
- `perspective_vk()` — Vulkan NDC (Y down, depth [0,1]) 用透視射影行列

### StageRenderer — 共通レイヤー

KuzuSurvivors の `src/render/stage_renderer.{h,cpp}` を移植し `IRenderLayer`
を実装する形に整えたもの。 最小 1 パイプライン (per-frame Scene UBO +
per-object push constants)、 既定 depth なし。 depth attachment を持つ
render pass を `set_render_pass()` で渡すと深度テスト/書き込みが有効になる。
「色付きキューブ群」を描くだけの軽量描画層で、 AC / KS 共通の仮表示・
フォールバック描画に使える。 drawable はゲーム側が `set_drawables()` で
毎フレーム差し込む (actor → StageDrawable 変換はゲーム側の責務)。

## 必要なデータ

- Pictor の `VulkanContext` (instance/device/swapchain/同期オブジェクト)
- StageRenderer 用 SPIR-V — `shaders/stage.vert.spv` / `shaders/stage.frag.spv`
  (GLSL ソースは `ergo/shaders/stage.{vert,frag}`、 build 時に glslc で bake)
- 環境変数 (任意): `ERGO_RENDER_SHADER_DIR` / `ERGO_RENDER_ASSET_ROOT`

## 依存

- C++17 標準ライブラリ: `<filesystem>` / `<mutex>` / `<condition_variable>` /
  `<atomic>` / `<vector>` / `<deque>` / `<cmath>`
- `Threads::Threads` (ScreenshotBridge の同期)
- Pictor + Vulkan SDK — **任意**。 `pictor` ターゲットが存在し Vulkan が
  見つかったときだけ実描画経路 (`FrameComposer::run_frame`、 StageRenderer の
  pipeline 構築) が有効化される (`ERGO_RENDER_HAS_VULKAN`)。 揃わない環境でも
  Vulkan 非依存部分 (カメラ math / asset path / ScreenshotBridge /
  FrameComposer のパス列管理) はビルド・テストできる
- テスト: mini-gtest (`ergo_gtest_main`)

## 設計判断

- Pictor は「生 Vulkan の `VulkanContext`」と「高レベルの `PictorRenderer`」の
  二重 API を持つが、 ergo_render は当面 **`VulkanContext` (生 Vulkan)** を
  基盤とし、 `PictorRenderer` は任意パススルー。
- 「実描画を Pictor データ層へ寄せる」のは今回スコープ外。
- ゲーム側の元ファイル (KS の `stage_renderer.{h,cpp}` 等) は今回変更しない。
  KS / AC への載せ替えは P2 で別途行う。

## 作業

### 入力

- ホスト: `add_pass()` でパス列を組み、 `initialize()` → 毎フレーム
  `run_frame(FrameContext)` → `shutdown()`
- HTTP スレッド: `ScreenshotBridge::request_and_wait()`

### 出力

- swapchain への描画 + present
- スクリーンショット RGBA バイト列 (ScreenshotBridge 経由)

### タスク

- フレームループ: acquire → on_first_frame (初回) → update → record → submit
  → present
- 初期化/破棄: レイヤー初期化は登録順、 破棄は逆順

## テスト

`tests/render/` (Vulkan 非依存部分のみ、 CI でも実行可能):

- `test_frame_context` — カメラ math (deg_to_rad / mat4_identity /
  look_at_rh / perspective_vk) と FrameContext の既定値
- `test_asset_paths` — 環境変数優先 / 起点直下検出 / 上方探索 /
  フォールバック / 末尾スラッシュ無し
- `test_screenshot_bridge` — タイムアウト / レンダースレッドによる充足 /
  失敗通知での即時解除 / consume の one-shot 性
- `test_frame_composer` — パス数追跡 / 初期化順 (登録順) / 破棄順 (逆順) /
  initialize・shutdown の冪等性 / VulkanContext 不在時の安全な縮退

Vulkan 実描画経路 (`run_frame` の acquire/record/submit/present、 StageRenderer
の pipeline 構築) は VulkanContext 実体を要するためユニットテストの対象外。
P2 で KS / AC へ載せ替えた際に実機で検証する。
