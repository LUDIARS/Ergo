# ergo_render 仕様

## 概要

`ergo_render` は **Pictor の上・ゲームの下** に入る横断オーケストレーション層。

LUDIARS の C++ ゲーム (PrivateGame / AdventureCube) は、 ゲーム World を
Pictor (低レベル Vulkan) に橋渡しする「WorldRenderer」相当を各自手書きし、
横断オーケストレーション — 初期化順序・パス構成・フレームループ・
submit/present・screenshot・破棄順序 — を 1 箇所に溜め込んで God Class 化して
いた。`ergo_render` はこの **横断層だけ** を引き取る共通モジュール。

各ゲームのサブレンダラ (描画プリミティブ) は既に分離できているので、
`ergo_render` は「横断オーケストレーション」だけを担う。 ゲーム固有の
サブレンダラ実装本体・actor→drawable 変換・FrameComposer のパス構成の
組み立ては `ergo_render` に入れず **ゲーム側に残す**。

依存の向きは **ergo_render → Pictor の一方向**。`ergo_render` はどのゲーム
(PrivateGame / AC) にも依存しないフレームワークであり、 上位レイヤー (Ergo の他
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
| `surface`    | `pictor::ISurfaceProvider*`   | 実描画では必須 | ウィンドウ/サーフェス供給者 (プラットフォーム中立) |
| `renderer`   | `pictor::PictorRenderer*`     | 任意 | 高レベルレンダラ (パススルー) |
| `anim`       | `pictor::AnimationSystem*`    | 任意 | skinning 行列ソース |
| `shader_dir` | `std::string`                 | —    | 解決済シェーダディレクトリ |
| `asset_root` | `std::string`                 | —    | 解決済アセットルート |

`surface` は Pictor の抽象インターフェース `ISurfaceProvider` を借用する。
実体はデスクトップが `GlfwSurfaceProvider`、 Android が
`AndroidSurfaceProvider` (`ANativeWindow`)、 iOS が `IOSSurfaceProvider`
(`CAMetalLayer`) だが、 **ergo_render の公開ヘッダは具象型を一切要求しない**
(KD-MOB-001)。 デスクトップの GLFW 経路も同じインターフェース越しに使う。

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

- PrivateGame の「postprocess 有効/無効 2 経路」 — 有効なら HDR scene パス + post パス
  + HUD パスの 3 列、 無効なら swapchain default 1 列、 と add_pass の呼び方を
  変えるだけで切り替わる。
- AC は最小構成 — default render pass 1 列に 1 レイヤーを載せるだけ。

framebuffer は `set_framebuffer_provider()` でパス毎に解決関数を登録できる
(post-process の HDR scene パスのような image index 非依存の固定 framebuffer
にも対応)。 未登録のパスは VulkanContext の default framebuffers を使う。

**パス間 hook / present 後 hook** — `IRenderLayer::record` は必ず
`vkCmdBeginRenderPass` の内側で呼ばれるが、 post-process チェーンや投影デカール
合成のように **自前で render pass を begin/end する合成処理** はレイヤーに
収まらない。 これを記録するため、 FrameComposer は 2 つの C 関数ポインタ
hook を持つ:

- `set_pre_pass_hook(pass_index, fn, user)` — パス `pass_index` を begin する
  直前、 render pass の外側で `fn(cmd, image_index, frame, user)` を呼ぶ。
  PrivateGame の post-process 経路は「HDR scene パス」と「HUD パス」の 2 パスを登録し、
  HUD パス (index 1) の pre-pass hook に「デカール合成 + post-process チェーン」
  を積む。
- `set_post_present_hook(fn, user)` — present 完了直後に
  `fn(image_index, frame, user)` を呼ぶ。 提示済み swapchain image を読み出す
  スクリーンショット処理などを積む。

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

PrivateGame の `src/render/stage_renderer.{h,cpp}` を移植し `IRenderLayer`
を実装する形に整えたもの。 最小 1 パイプライン (per-frame Scene UBO +
per-object push constants)、 既定 depth なし。 depth attachment を持つ
render pass を `set_render_pass()` で渡すと深度テスト/書き込みが有効になる。
「色付きキューブ群」を描くだけの軽量描画層で、 AC / PrivateGame 共通の仮表示・
フォールバック描画に使える。 drawable はゲーム側が `set_drawables()` で
毎フレーム差し込む (actor → StageDrawable 変換はゲーム側の責務)。

### (F) RenderBackend — 実描画バックエンドのビルド時契約

`render_backend.h` は「このビルドに実描画経路が入っているか」「どの
プラットフォーム契約で組まれたか」を実行時に問い合わせる読み取り専用 API。

```cpp
enum class RenderPlatform      : uint8_t { Unknown, Desktop, Android, IOS };
enum class RenderBackendError  : uint8_t {
    None, VulkanBackendUnavailable, FrameComposerNotInitialized, VulkanContextMissing,
    SurfaceProviderMissing, SurfaceNotReady, FrameSubmissionFailed };
struct RenderBackendContract {
    bool           real_render_enabled;
    RenderPlatform platform;
    const char*    vulkan_source;   // "Vulkan SDK" / "Android NDK" / "MoltenVK"
};
RenderBackendContract render_backend_contract();
bool                  real_render_enabled();
```

実描画無効ビルドは `platform == Unknown` / `vulkan_source == "none"` を返し、
**desktop を名乗らない**。 「Vulkan 非依存ビルドなのに Desktop 構成として
成功している」状態を作らないための取り決め。

### (G) RenderRequirements — RenderContext の型付き前提検査

`render_requirements.h` は「いま渡された `RenderContext` で実際に描けるか」を
判定し、 失敗を必ず `RenderBackendError` で返す。 判定順は
バックエンド不在 → `vk` null → `surface` null → サーフェス未準備。

```cpp
RenderBackendError check_render_requirements(const RenderContext& ctx);
bool               surface_has_native_window(const RenderContext& ctx);
```

`surface_has_native_window()` は `ISurfaceProvider::get_native_handle().type`
を見るため、 GLFW / `ANativeWindow` / `CAMetalLayer` を区別せずに扱える。
Android のバックグラウンド遷移 (`ANativeWindow` 破棄) は `SurfaceNotReady`
として表れる。

`FrameComposer::initialize()` はこの判定結果を戻り値として返し、
`last_error()` に保持する。 初期化済みの状態で `initialize()` を再度呼んだ
場合は no-op で、 **初回 initialize 時の**診断結果を返す (その後の
`run_frame()` が観測した一時的な失敗は混ぜない — そちらは `last_error()`)。
記録 / submit の Vulkan 呼び出しが失敗した場合は `FrameSubmissionFailed` を
`last_error()` に残してから継続不能を返す (成功値のまま落とさない)。
`run_frame()` はフレームごとに同じ前提を再検査し、
Android のバックグラウンド遷移などで surface が失われた場合にも Vulkan 呼び出し
の前にそのフレームをスキップし、ループは継続して surface の復帰を待つ。
initialize 前に `run_frame()` を呼んだ場合は
`FrameComposerNotInitialized` を返す。 レイヤーの initialize / shutdown の順序と
所有関係は従来どおり (登録順に初期化、 逆順に破棄) で変更していない。
実描画不可を「Vulkan-free の成功」へ縮退させないことだけが変更点。

### (H) クロスプラットフォームビルド契約

実描画可否の判定正本は **`pictor` ターゲットが公開する
`PICTOR_HAS_VULKAN`** であり、 デスクトップ専用 import ターゲット
`Vulkan::Vulkan` の有無ではない。 判定は
`cmake/ErgoRenderBackend.cmake` の `ergo_render_resolve_backend()` に集約する。

| プラットフォーム | Vulkan の出所 | ergo_render のリンク | 付与される定義 |
|---|---|---|---|
| Desktop (SDK) | Vulkan SDK (`find_package(Vulkan)`) | `pictor` + `Vulkan::Vulkan` | `ERGO_RENDER_PLATFORM_DESKTOP=1` |
| Desktop (host supplied) | pictor が供給する Vulkan | `pictor` のみ | `ERGO_RENDER_PLATFORM_DESKTOP=1` |
| Android | NDK 同梱 `libvulkan.so` (pictor がリンク) | `pictor` のみ | `ERGO_RENDER_PLATFORM_ANDROID=1` |
| iOS | MoltenVK (ホスト xcodeproj / `Vulkan_LIBRARIES`) | `pictor` のみ | `ERGO_RENDER_PLATFORM_IOS=1` |

有効化時は共通で `ERGO_RENDER_HAS_VULKAN=1` と
`ERGO_RENDER_VULKAN_SOURCE="<出所>"` が付く。

実描画を確保できなかった構成の扱い (`ergo_render_enforce_backend()`):

- Android / iOS — **常に構成エラー** (`FATAL_ERROR`)。 モバイルには
  Vulkan-free の正当な用途が無いため、 黙って縮退させない。
- Desktop — 既定は `WARNING` (Vulkan 非依存部分のユニットテスト構成を
  残すため)。 `-DERGO_RENDER_REQUIRE_REAL=ON` で構成エラーへ格上げできる。
- `glslc` の有無は SPIR-V bake の可否だけに影響し、 実描画可否の判定材料に
  しない (モバイルは bake 済み SPIR-V をホストのパッケージが配る)。

## 必要なデータ

- Pictor の `VulkanContext` (instance/device/swapchain/同期オブジェクト)
- StageRenderer 用 SPIR-V — `shaders/stage.vert.spv` / `shaders/stage.frag.spv`
  (GLSL ソースは `ergo/shaders/stage.{vert,frag}`、 build 時に glslc で bake)
- 環境変数 (任意): `ERGO_RENDER_SHADER_DIR` / `ERGO_RENDER_ASSET_ROOT`

## 依存

- C++17 標準ライブラリ: `<filesystem>` / `<mutex>` / `<condition_variable>` /
  `<atomic>` / `<vector>` / `<deque>` / `<cmath>`
- `Threads::Threads` (ScreenshotBridge の同期)
- Pictor + Vulkan — デスクトップでは **任意**。 `pictor` ターゲットが
  `PICTOR_HAS_VULKAN` を公開していれば実描画経路
  (`FrameComposer::run_frame`、 StageRenderer の pipeline 構築) が有効化される
  (`ERGO_RENDER_HAS_VULKAN`)。 揃わないデスクトップ環境でも Vulkan 非依存部分
  (カメラ math / asset path / ScreenshotBridge / FrameComposer のパス列管理)
  はビルド・テストできる。 Android / iOS は実描画が必須で、 確保できない
  構成は構成エラーになる (上記 (H))
- テスト: mini-gtest (`ergo_gtest_main`)

## 設計判断

- Pictor は「生 Vulkan の `VulkanContext`」と「高レベルの `PictorRenderer`」の
  二重 API を持つが、 ergo_render は当面 **`VulkanContext` (生 Vulkan)** を
  基盤とし、 `PictorRenderer` は任意パススルー。
- 「実描画を Pictor データ層へ寄せる」のは今回スコープ外。
- ゲーム側の元ファイル (PrivateGame の `stage_renderer.{h,cpp}` 等) は今回変更しない。
  PrivateGame / AC への載せ替えは P2 で別途行う。

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
- `test_render_backend` — バックエンド契約とビルド構成の一致 /
  プラットフォーム名・エラー名の安定性 / 空 `RenderContext` が決して
  成功を返さないこと / `FrameComposer::initialize` と `run_frame` が
  型付き失敗を返し `last_error()` に残すこと / initialize 前の `run_frame()` が
  `FrameComposerNotInitialized` を返すこと / `initialize()` の再呼び出しが
  初回の診断結果を返すこと

Vulkan 実描画経路 (`run_frame` の acquire/record/submit/present、 StageRenderer
の pipeline 構築) は VulkanContext 実体を要するためユニットテストの対象外。
P2 で PrivateGame / AC へ載せ替えた際に実機で検証する。
