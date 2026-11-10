#pragma once
/// The Studio window: GLFW surface (Pictor GlfwSurfaceProvider), Pictor
/// VulkanContext, ergo_render FrameComposer with one UiLayer, driving a
/// ui::StudioApp. run() blocks until the window closes or `stop` is set.
#include "ergo/studio/host/plugin_host.h"

#include <atomic>
#include <functional>
#include <string>

namespace ergo::studio::shell {

struct WindowConfig {
    std::string title = "Ergo Studio";
    int         width  = 1280;
    int         height = 800;
    std::string font_path;    ///< TTF / OTF / TTC
    std::string shader_dir;   ///< holds ui_widget.vert.spv / ui_widget.frag.spv
};

/// Opens the window and runs the UI loop. Returns 0 on a normal close, or a
/// non-zero code with `err` set when the window / Vulkan / font cannot start.
int run_studio_window(host::PluginHost& host, const WindowConfig& cfg, const std::atomic<bool>& stop, std::string& err);

} // namespace ergo::studio::shell
