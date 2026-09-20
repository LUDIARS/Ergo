#include "gtest/gtest.h"

#include "ergo/render/frame_composer.h"
#include "ergo/render/render_backend.h"
#include "ergo/render/render_context.h"
#include "ergo/render/render_requirements.h"

#include <string>

using namespace ergo::render;

// KD-MOB-001 の回帰テスト。 実行環境が Vulkan を持たない CI でも成立するよう、
// 「実描画が有効なビルドか」で期待値を分岐させる。 共通の不変条件は
// 「実描画不可を暗黙の成功へ縮退させない」こと。

TEST(RenderBackend, ContractMatchesBuildConfiguration) {
    const RenderBackendContract contract = render_backend_contract();
    EXPECT_EQ(contract.real_render_enabled, real_render_enabled());

    if (contract.real_render_enabled) {
        // 実描画ビルドは必ずプラットフォーム契約と Vulkan の出所を名乗る。
        EXPECT_NE(contract.platform, RenderPlatform::Unknown);
        EXPECT_NE(std::string(contract.vulkan_source), "none");
    } else {
        // Vulkan-free ビルドは desktop/android/ios のいずれも名乗らない。
        EXPECT_EQ(contract.platform, RenderPlatform::Unknown);
        EXPECT_EQ(std::string(contract.vulkan_source), "none");
    }
}

TEST(RenderBackend, PlatformNamesAreStable) {
    EXPECT_EQ(std::string(to_string(RenderPlatform::Desktop)), "desktop");
    EXPECT_EQ(std::string(to_string(RenderPlatform::Android)), "android");
    EXPECT_EQ(std::string(to_string(RenderPlatform::IOS)),     "ios");
    EXPECT_EQ(std::string(to_string(RenderPlatform::Unknown)), "unknown");
}

TEST(RenderBackend, ErrorNamesAreStable) {
    EXPECT_EQ(std::string(to_string(RenderBackendError::None)), "none");
    EXPECT_EQ(std::string(to_string(RenderBackendError::VulkanBackendUnavailable)),
              "vulkan-backend-unavailable");
    EXPECT_EQ(std::string(to_string(RenderBackendError::FrameComposerNotInitialized)),
              "frame-composer-not-initialized");
    EXPECT_EQ(std::string(to_string(RenderBackendError::VulkanContextMissing)),
              "vulkan-context-missing");
    EXPECT_EQ(std::string(to_string(RenderBackendError::SurfaceProviderMissing)),
              "surface-provider-missing");
    EXPECT_EQ(std::string(to_string(RenderBackendError::SurfaceNotReady)),
              "surface-not-ready");
    EXPECT_EQ(std::string(to_string(RenderBackendError::FrameSubmissionFailed)),
              "frame-submission-failed");
}

TEST(RenderRequirements, EmptyContextNeverReportsSuccess) {
    RenderContext ctx;   // vk / surface とも nullptr
    const RenderBackendError err = check_render_requirements(ctx);
    EXPECT_NE(err, RenderBackendError::None);

    if (real_render_enabled()) {
        // 実描画ビルドなら「VulkanContext が無い」が先に立つ。
        EXPECT_EQ(err, RenderBackendError::VulkanContextMissing);
    } else {
        // Vulkan-free ビルドはバックエンド自体が無いと明示する。
        EXPECT_EQ(err, RenderBackendError::VulkanBackendUnavailable);
    }
}

TEST(RenderRequirements, SurfaceIsNotReadyWithoutProvider) {
    RenderContext ctx;
    EXPECT_FALSE(surface_has_native_window(ctx));
}

TEST(FrameComposer, InitializeReportsTypedFailureForInvalidContext) {
    FrameComposer fc;
    RenderContext ctx;
    fc.add_pass(VK_NULL_HANDLE, {});

    const RenderBackendError err = fc.initialize(ctx);
    EXPECT_NE(err, RenderBackendError::None);
    EXPECT_EQ(err, fc.last_error());
    EXPECT_EQ(err, check_render_requirements(ctx));
}

TEST(FrameComposer, RunFrameWithInvalidContextKeepsTypedFailure) {
    FrameComposer fc;
    RenderContext ctx;
    fc.add_pass(VK_NULL_HANDLE, {});
    fc.initialize(ctx);

    FrameContext frame;
    EXPECT_FALSE(fc.run_frame(frame));
    EXPECT_NE(fc.last_error(), RenderBackendError::None);
    EXPECT_EQ(fc.frame_count(), 0u);
}

// run_frame() は last_error_ を毎フレーム上書きするが、 initialize() の
// 再呼び出しは「初期化時の診断」を返し続ける (直近フレームの一時的な失敗を
// 初期化の結果として混ぜない)。
TEST(FrameComposer, ReinitializeReturnsInitializeTimeDiagnosis) {
    FrameComposer fc;
    RenderContext ctx;
    fc.add_pass(VK_NULL_HANDLE, {});

    const RenderBackendError first = fc.initialize(ctx);

    FrameContext frame;
    fc.run_frame(frame);   // last_error_ を上書きしうる

    EXPECT_EQ(fc.initialize(ctx), first);
}

TEST(FrameComposer, RunFrameBeforeInitializeReportsTypedFailure) {
    FrameComposer fc;
    FrameContext frame;

    EXPECT_FALSE(fc.run_frame(frame));
    EXPECT_EQ(fc.last_error(), RenderBackendError::FrameComposerNotInitialized);
}
