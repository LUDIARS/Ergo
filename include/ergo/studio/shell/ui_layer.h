#pragma once
/// ergo_render layer that draws the Studio UI: ui_kit draw commands become
/// Pictor UIDrawCmds, and all text goes through one overlay texture
/// (TextCanvas) drawn last. The overlay is created once at the size of the
/// largest surface the window can get (the monitor) because UIRenderer
/// textures cannot be released (64 slots).
#include "ergo/render/render_layer.h"
#include "ergo/studio/shell/text_canvas.h"

#include <memory>
#include <string>
#include <vector>

namespace ergo::studio::shell {

class UiLayer final : public render::IRenderLayer {
public:
    UiLayer(const FontAtlas& font, uint32_t overlay_w, uint32_t overlay_h, std::string shader_dir);
    ~UiLayer() override;

    /// The frame's UI (copied; set before FrameComposer::run_frame()).
    void set_draw_list(const std::vector<ui_kit::DrawCmd>& list) { list_ = list; }
    /// Empty when ready; otherwise why the UI cannot be drawn.
    const std::string& error() const { return error_; }

    void initialize(render::RenderContext& ctx) override;
    void set_render_pass(VkRenderPass render_pass) override;
    void update(const render::FrameContext& frame) override;
    void record(VkCommandBuffer cmd, VkExtent2D extent) override;
    void shutdown() override;

private:
    struct Impl;
    std::unique_ptr<Impl>         impl_;
    const FontAtlas&              font_;
    std::vector<ui_kit::DrawCmd>  list_;
    std::string                   shader_dir_;
    std::string                   error_;
};

} // namespace ergo::studio::shell
