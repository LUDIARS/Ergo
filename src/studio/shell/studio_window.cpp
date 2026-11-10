#include "ergo/studio/shell/studio_window.h"

#include "ergo/render/frame_composer.h"
#include "ergo/render/render_context.h"
#include "ergo/studio/shell/font_atlas.h"
#include "ergo/studio/shell/ui_layer.h"
#include "ergo/studio/ui/studio_app.h"
#include "pictor/surface/glfw_surface_provider.h"
#include "pictor/surface/vulkan_context.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>

namespace ergo::studio::shell {
namespace {

/// Wheel steps since the last frame (one window, so one counter).
std::atomic<int> g_scroll_tenths{0};

void on_scroll(GLFWwindow*, double, double dy) { g_scroll_tenths += static_cast<int>(dy * 10); }

} // namespace

int run_studio_window(host::PluginHost& host, const WindowConfig& cfg, const std::atomic<bool>& stop, std::string& err) {
    FontAtlas font;
    if (!font.load(cfg.font_path, err)) return 2;

    pictor::GlfwSurfaceProvider surface;
    pictor::GlfwWindowConfig wc;
    wc.width  = static_cast<uint32_t>(cfg.width);
    wc.height = static_cast<uint32_t>(cfg.height);
    wc.title  = cfg.title;
    if (!surface.create(wc)) {
        err = "window could not be created (GLFW)";
        return 3;
    }
    GLFWwindow* win = surface.glfw_window();
    glfwSetScrollCallback(win, &on_scroll);

    pictor::VulkanContext vk;
    if (!vk.initialize(&surface)) {
        err = "Vulkan could not start: " + vk.init_result().detail;
        return 4;
    }

    // The overlay texture is created once (UIRenderer cannot release textures):
    // size it for the largest framebuffer the window can get.
    uint32_t ow = vk.swapchain_extent().width, oh = vk.swapchain_extent().height;
    if (const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor())) {
        ow = std::max(ow, static_cast<uint32_t>(mode->width));
        oh = std::max(oh, static_cast<uint32_t>(mode->height));
    }
    UiLayer layer(font, ow, oh, cfg.shader_dir);

    render::RenderContext rc;
    rc.vk         = &vk;
    rc.surface    = &surface;
    rc.shader_dir = cfg.shader_dir;
    render::FrameComposer composer;
    composer.add_pass(vk.default_render_pass(), {&layer});
    const auto init = composer.initialize(rc);
    if (init != render::RenderBackendError::None) {
        err = std::string("render setup failed: ") + render::to_string(init);
        composer.shutdown();
        vk.shutdown();
        return 5;
    }
    if (!layer.error().empty()) {
        err = layer.error();
        composer.shutdown();
        vk.shutdown();
        return 6;
    }

    ui::StudioApp app(host, font);
    auto last = std::chrono::steady_clock::now();
    int code = 0;
    while (!stop.load() && !surface.should_close()) {
        surface.poll_events();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        int fbw = 0, fbh = 0, ww = 0, wh = 0;
        glfwGetFramebufferSize(win, &fbw, &fbh);
        glfwGetWindowSize(win, &ww, &wh);
        if (fbw == 0 || fbh == 0) continue;   // minimised
        double cx = 0, cy = 0;
        glfwGetCursorPos(win, &cx, &cy);
        // Cursor positions are in window coordinates; the UI is laid out in framebuffer pixels.
        const float sx = ww > 0 ? static_cast<float>(fbw) / ww : 1, sy = wh > 0 ? static_cast<float>(fbh) / wh : 1;
        ui::InputState in;
        in.x      = static_cast<float>(cx) * sx;
        in.y      = static_cast<float>(cy) * sy;
        in.down   = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        in.scroll = static_cast<float>(g_scroll_tenths.exchange(0)) / 10.0f;

        app.set_viewport(fbw, fbh);
        app.frame(in, dt);
        layer.set_draw_list(app.draw_list());

        render::FrameContext frame;
        frame.dt     = dt;
        frame.extent = {static_cast<uint32_t>(fbw), static_cast<uint32_t>(fbh)};
        if (!composer.run_frame(frame)) {
            err  = std::string("frame failed: ") + render::to_string(composer.last_error());
            code = 7;
            break;
        }
    }
    vk.device_wait_idle();
    composer.shutdown();
    vk.shutdown();
    surface.destroy();
    return code;
}

} // namespace ergo::studio::shell
