#include "ergo/render/render_backend.h"

namespace ergo::render {

namespace {

constexpr RenderPlatform build_platform() {
#if defined(ERGO_RENDER_PLATFORM_ANDROID)
    return RenderPlatform::Android;
#elif defined(ERGO_RENDER_PLATFORM_IOS)
    return RenderPlatform::IOS;
#elif defined(ERGO_RENDER_PLATFORM_DESKTOP)
    return RenderPlatform::Desktop;
#else
    return RenderPlatform::Unknown;
#endif
}

constexpr bool build_has_real_render() {
#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
    return true;
#else
    return false;
#endif
}

constexpr const char* build_vulkan_source() {
#if defined(ERGO_RENDER_VULKAN_SOURCE)
    return ERGO_RENDER_VULKAN_SOURCE;
#else
    return "none";
#endif
}

} // namespace

RenderBackendContract render_backend_contract() {
    RenderBackendContract contract;
    contract.real_render_enabled = build_has_real_render();
    // 実描画無効ビルドでプラットフォーム契約を名乗らない
    // (「Vulkan 無しの Desktop ビルド」を成功扱いさせないため)。
    contract.platform      = contract.real_render_enabled ? build_platform()
                                                          : RenderPlatform::Unknown;
    contract.vulkan_source = contract.real_render_enabled ? build_vulkan_source()
                                                          : "none";
    return contract;
}

bool real_render_enabled() {
    return build_has_real_render();
}

const char* to_string(RenderPlatform platform) {
    switch (platform) {
        case RenderPlatform::Desktop: return "desktop";
        case RenderPlatform::Android: return "android";
        case RenderPlatform::IOS:     return "ios";
        case RenderPlatform::Unknown: break;
    }
    return "unknown";
}

const char* to_string(RenderBackendError error) {
    switch (error) {
        case RenderBackendError::None:
            return "none";
        case RenderBackendError::VulkanBackendUnavailable:
            return "vulkan-backend-unavailable";
        case RenderBackendError::FrameComposerNotInitialized:
            return "frame-composer-not-initialized";
        case RenderBackendError::VulkanContextMissing:
            return "vulkan-context-missing";
        case RenderBackendError::SurfaceProviderMissing:
            return "surface-provider-missing";
        case RenderBackendError::SurfaceNotReady:
            return "surface-not-ready";
        case RenderBackendError::FrameSubmissionFailed:
            return "frame-submission-failed";
    }
    return "unknown";
}

} // namespace ergo::render
