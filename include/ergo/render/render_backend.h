#pragma once

/// ergo_render — 実描画バックエンドの **ビルド時契約**。
///
/// ergo_render は Pictor (生 Vulkan) の上に乗るオーケストレーション層で、
/// 実描画経路が有効かどうかはビルド構成で決まる:
///
/// | プラットフォーム | Vulkan の出所                      | 供給元                     |
/// |---|---|---|
/// | Desktop (Win/Linux/macOS) | Vulkan SDK または pictor 供給 | `Vulkan::Vulkan` または `pictor` |
/// | Android                   | NDK 同梱 `libvulkan.so`             | `pictor` (NDK)     |
/// | iOS                       | MoltenVK (Vulkan→Metal)             | `pictor` (ホスト)  |
///
/// Android / iOS ではデスクトップの `Vulkan::Vulkan` import ターゲットが
/// 存在しない。 「`Vulkan::Vulkan` が無い = 描画不可」と判定すると
/// モバイルが黙って Vulkan-free ビルドへ落ちるため、 ergo_render は
/// **pictor ターゲットが公開する runtime Vulkan contract**
/// (`PICTOR_HAS_VULKAN`) を実描画可否の正本とする
/// (`cmake/ErgoRenderBackend.cmake`)。
///
/// このヘッダはその結果 (どのプラットフォーム契約でビルドされたか、
/// 実描画が有効か) を実行時に問い合わせる読み取り専用 API を提供する。
/// 判定結果を **暗黙の成功へ縮退させない** ため、 失敗は必ず
/// `RenderBackendError` の型付き値として返す。

#include <cstdint>

namespace ergo::render {

/// ergo_render がビルドされたプラットフォーム契約。
enum class RenderPlatform : std::uint8_t {
    /// プラットフォーム契約が解決できなかった (実描画無効ビルド)。
    Unknown = 0,
    /// デスクトップ — Vulkan SDK + ウィンドウシステム (GLFW 等)。
    Desktop = 1,
    /// Android — NDK 同梱 Vulkan + `ANativeWindow`。
    Android = 2,
    /// iOS — MoltenVK + `CAMetalLayer`。
    IOS     = 3,
};

/// 実描画が成立しない理由。 `None` 以外はすべて **明示的な失敗** であり、
/// 呼び出し側がこれを握り潰して「描けた」ことにしてはならない。
enum class RenderBackendError : std::uint8_t {
    /// 失敗なし。
    None = 0,
    /// このビルドに実描画経路が入っていない (pictor 不在 / Vulkan 契約不成立)。
    VulkanBackendUnavailable = 1,
    /// `RenderContext::vk` が null。
    VulkanContextMissing = 2,
    /// `RenderContext::surface` が null (サーフェス供給者が渡されていない)。
    SurfaceProviderMissing = 3,
    /// サーフェス供給者はいるが、 いまネイティブウィンドウを持っていない
    /// (Android のバックグラウンド遷移など)。 一時的な状態で回復しうる。
    SurfaceNotReady = 4,
    /// `FrameComposer::run_frame()` が initialize() 前に呼ばれた。
    FrameComposerNotInitialized = 5,
    /// フレームの記録 / submit で Vulkan 呼び出しが失敗した
    /// (command buffer の begin/end、 vkQueueSubmit)。 前提は満たしていたが
    /// 実行時に落ちた場合で、 `run_frame()` は継続不能 (false) を返す。
    FrameSubmissionFailed = 6,
};

/// ビルド時に解決された実描画バックエンド契約。
struct RenderBackendContract {
    /// 実描画経路 (FrameComposer の run_frame / StageRenderer の pipeline)
    /// がこのビルドに含まれているか。
    bool real_render_enabled = false;

    /// どのプラットフォーム契約でビルドされたか。
    RenderPlatform platform = RenderPlatform::Unknown;

    /// Vulkan シンボルの出所を示す人間可読な名前
    /// (例: `"Vulkan SDK"` / `"Android NDK"` / `"MoltenVK"` / `"none"`)。
    const char* vulkan_source = "none";
};

/// このビルドの実描画バックエンド契約を返す。
RenderBackendContract render_backend_contract();

/// 実描画経路が有効なビルドか。
/// `render_backend_contract().real_render_enabled` の短縮形。
bool real_render_enabled();

const char* to_string(RenderPlatform platform);
const char* to_string(RenderBackendError error);

} // namespace ergo::render
