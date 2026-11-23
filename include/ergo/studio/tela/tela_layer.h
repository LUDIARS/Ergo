#pragma once
/// ergo_render layer that draws one tela::Runtime with Tela's GPU backend
/// (Tela::Gpu GpuRenderer on Pictor's UIRenderer, SPEC-TL-GPU). The window
/// keeps GLFW, the swapchain and the frame loop; this layer prepares the
/// frame in update() (outside the render pass) and records it in record().
#include "ergo/render/render_layer.h"

#include <tela/gpu_renderer.hpp>
#include <tela/runtime.hpp>

#include <memory>
#include <string>

namespace ergo::studio::tela_shell {

class TelaLayer final : public render::IRenderLayer {
public:
    /// `runtime` outlives the layer. `font_path` is a TrueType / OpenType file;
    /// `shader_dir` holds Pictor's ui_widget.vert.spv / ui_widget.frag.spv.
    TelaLayer(::tela::Runtime& runtime, std::string font_path, std::string shader_dir);
    ~TelaLayer() override;

    /// Empty when ready; otherwise why the UI cannot be drawn.
    const std::string& error() const { return error_; }
    /// True when the last update() prepared a frame different from the one before.
    bool changed() const { return changed_; }

    void initialize(render::RenderContext& ctx) override;
    void set_render_pass(VkRenderPass render_pass) override;
    void update(const render::FrameContext& frame) override;
    void record(VkCommandBuffer cmd, VkExtent2D extent) override;
    void shutdown() override;

private:
    ::tela::Runtime&                     runtime_;
    std::string                          font_path_;
    std::string                          shader_dir_;
    render::RenderContext*               ctx_ = nullptr;
    std::unique_ptr<::tela::GpuRenderer> renderer_;
    std::string                          error_;
    bool                                 changed_ = false;
};

} // namespace ergo::studio::tela_shell
