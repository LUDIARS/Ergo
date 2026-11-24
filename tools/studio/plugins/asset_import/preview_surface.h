#pragma once

#include "pictor/surface/surface_provider.h"

struct GLFWwindow;

namespace ergo::studio::plugins::asset_import {

// GLFW is owned by the Studio launcher. This surface owns only its own window.
class PreviewSurface final : public pictor::ISurfaceProvider {
public:
    PreviewSurface();
    ~PreviewSurface() override;
    PreviewSurface(const PreviewSurface&) = delete;
    PreviewSurface& operator=(const PreviewSurface&) = delete;
    pictor::NativeWindowHandle get_native_handle() const override;
    pictor::SwapchainConfig get_swapchain_config() const override;
    bool should_close() const override;
    uint32_t get_required_instance_extensions(const char** names, uint32_t capacity) const override;
    GLFWwindow* window() const { return window_; }

private:
    GLFWwindow* window_ = nullptr;
};

} // namespace ergo::studio::plugins::asset_import
