#pragma once
/// The Studio window on Tela: GLFW surface (Pictor GlfwSurfaceProvider),
/// Pictor VulkanContext, ergo_render FrameComposer with one TelaLayer. The
/// window is one more screen of the ScreenHub (its own tela::Runtime), and its
/// loop pumps the hub, so browser pages are served on the same UI thread.
#include "ergo/studio/tela/screen_hub.h"

#include <atomic>
#include <functional>
#include <string>

namespace ergo::studio::tela_shell {

struct TelaWindowConfig {
    std::string title  = "Ergo Studio";
    int         width  = 1280;
    int         height = 800;
    std::string font_path;    ///< TrueType / OpenType file
    std::string shader_dir;   ///< holds ui_widget.vert.spv / ui_widget.frag.spv
    std::function<void()> on_frame;
    std::function<void()> before_close;
};

/// Opens the window and runs the UI loop until the window closes or `stop`
/// is set. Returns 0 on a normal close, or a non-zero code with `err` set
/// when the window / Vulkan / renderer cannot start.
int run_tela_window(ScreenHub& hub, const TelaWindowConfig& cfg, const std::atomic<bool>& stop, std::string& err);

} // namespace ergo::studio::tela_shell
