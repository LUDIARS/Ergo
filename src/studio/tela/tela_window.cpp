#include "ergo/studio/tela/tela_window.h"

#include "ergo/render/frame_composer.h"
#include "ergo/render/render_context.h"
#include "ergo/studio/tela/glfw_tela_input.h"
#include "ergo/studio/tela/tela_layer.h"
#include "pictor/surface/glfw_surface_provider.h"
#include "pictor/surface/vulkan_context.h"

#include <GLFW/glfw3.h>

#include <chrono>

namespace ergo::studio::tela_shell {

int run_tela_window(ScreenHub& hub, const TelaWindowConfig& cfg, const std::atomic<bool>& stop, std::string& err) {
    pictor::GlfwSurfaceProvider surface;
    pictor::GlfwWindowConfig    wc;
    wc.width  = static_cast<uint32_t>(cfg.width);
    wc.height = static_cast<uint32_t>(cfg.height);
    wc.title  = cfg.title;
    if (!surface.create(wc)) {
        err = "window could not be created (GLFW)";
        return 3;
    }
    pictor::VulkanContext vk;
    if (!vk.initialize(&surface)) {
        err = "Vulkan could not start: " + vk.init_result().detail;
        return 4;
    }

    int code = 0;
    {
        ::tela::Runtime runtime;
        TelaLayer       layer(runtime, cfg.font_path, cfg.shader_dir);
        render::RenderContext rc;
        rc.vk         = &vk;
        rc.surface    = &surface;
        rc.shader_dir = cfg.shader_dir;
        render::FrameComposer composer;
        composer.add_pass(vk.default_render_pass(), {&layer});
        const auto init = composer.initialize(rc);
        if (init != render::RenderBackendError::None) {
            err  = std::string("render setup failed: ") + render::to_string(init);
            code = 5;
        } else if (!layer.error().empty()) {
            err  = layer.error();
            code = 6;
        }
        if (code == 0) {
            GlfwTelaInput input(surface.glfw_window(), runtime);
            hub.add(runtime, ScreenAccess::window);   // the window is the launcher (the tabs are on /shell/)
            auto last   = std::chrono::steady_clock::now();
            bool redraw = true;   // the swapchain needs an image even when the runtime did not change
            while (!stop.load() && !surface.should_close()) {
                // Sleep until input arrives; the timeout keeps the browser pages and health fresh.
                glfwWaitEventsTimeout(1.0 / 60.0);
                const auto  now = std::chrono::steady_clock::now();
                const float dt  = std::chrono::duration<float>(now - last).count();
                last            = now;
                redraw          = input.synchronize() || redraw;
                hub.pump(dt);
                if (!runtime.viewport().visible || (!runtime.needs_frame() && !redraw)) continue;
                render::FrameContext frame;
                frame.dt     = dt;
                frame.extent = {static_cast<uint32_t>(runtime.viewport().width),
                                static_cast<uint32_t>(runtime.viewport().height)};
                if (!composer.run_frame(frame)) {
                    err  = std::string("frame failed: ") + render::to_string(composer.last_error());
                    code = 7;
                    break;
                }
                runtime.frame_presented();
                redraw = false;
            }
            hub.remove(runtime);
        }
        vk.device_wait_idle();
        composer.shutdown();
    }
    vk.shutdown();
    surface.destroy();
    return code;
}

} // namespace ergo::studio::tela_shell
