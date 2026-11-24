#include "preview_surface.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {

PreviewSurface::PreviewSurface() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(512, 512, "Ergo Studio — Visus", nullptr, nullptr);
    if (!window_) throw std::runtime_error("could not create Visus preview window");
}

PreviewSurface::~PreviewSurface() { glfwDestroyWindow(window_); }

pictor::NativeWindowHandle PreviewSurface::get_native_handle() const {
    pictor::NativeWindowHandle handle;
    using Type = pictor::NativeWindowHandle::Type;
#ifdef _WIN32
    handle.type = Type::Win32;
    handle.win32 = {glfwGetWin32Window(window_), GetModuleHandle(nullptr)};
#elif defined(__APPLE__)
    handle.type = Type::Cocoa;
    handle.cocoa.ns_view = glfwGetCocoaWindow(window_);
#elif defined(__linux__)
    handle.type = Type::Xlib;
    handle.xlib = {glfwGetX11Display(), glfwGetX11Window(window_)};
#endif
    return handle;
}

pictor::SwapchainConfig PreviewSurface::get_swapchain_config() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return {static_cast<uint32_t>(std::max(0, width)), static_cast<uint32_t>(std::max(0, height)), true, 2};
}

bool PreviewSurface::should_close() const { return glfwWindowShouldClose(window_) != 0; }

uint32_t PreviewSurface::get_required_instance_extensions(const char** names, uint32_t capacity) const {
    uint32_t count = 0;
    const auto extensions = glfwGetRequiredInstanceExtensions(&count);
    if (!extensions) return 0;
    count = std::min(count, capacity);
    std::copy_n(extensions, count, names);
    return count;
}

} // namespace ergo::studio::plugins::asset_import
