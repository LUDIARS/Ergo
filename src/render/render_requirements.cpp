#include "ergo/render/render_requirements.h"

#include "ergo/render/render_context.h"

#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
#include "pictor/surface/surface_provider.h"
#endif

namespace ergo::render {

bool surface_has_native_window(const RenderContext& ctx) {
#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
    if (!ctx.surface) return false;
    // GLFW / ANativeWindow / CAMetalLayer のどれであっても、 供給者が
    // 有効なハンドルを持っているかは NativeWindowHandle::type で分かる。
    return ctx.surface->get_native_handle().type
           != pictor::NativeWindowHandle::Type::None;
#else
    (void)ctx;
    return false;
#endif
}

RenderBackendError check_render_requirements(const RenderContext& ctx) {
    if (!real_render_enabled()) {
        return RenderBackendError::VulkanBackendUnavailable;
    }
    if (!ctx.vk) {
        return RenderBackendError::VulkanContextMissing;
    }
    if (!ctx.surface) {
        return RenderBackendError::SurfaceProviderMissing;
    }
    if (!surface_has_native_window(ctx)) {
        return RenderBackendError::SurfaceNotReady;
    }
    return RenderBackendError::None;
}

} // namespace ergo::render
