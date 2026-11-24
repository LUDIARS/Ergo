#include "ergo/studio/tela/tela_layer.h"

#include "ergo/render/render_context.h"
#include "pictor/surface/vulkan_context.h"

#include <exception>
#include <utility>

namespace ergo::studio::tela_shell {

TelaLayer::TelaLayer(::tela::Runtime& runtime, std::string font_path, std::string shader_dir)
    : runtime_(runtime), font_path_(std::move(font_path)), shader_dir_(std::move(shader_dir)) {}

TelaLayer::~TelaLayer() { shutdown(); }

void TelaLayer::initialize(render::RenderContext& ctx) {
    ctx_ = &ctx;
    if (!ctx.vk) error_ = "no Vulkan context";
}

void TelaLayer::set_render_pass(VkRenderPass render_pass) {
    if (!error_.empty() || !ctx_ || !ctx_->vk) return;
    renderer_.reset();   // a new pass (swapchain recreated) needs a new pipeline
    try {
        ::tela::GpuTarget target{*ctx_->vk, render_pass, ctx_->vk->swapchain_format(), shader_dir_};
        renderer_ = std::make_unique<::tela::GpuRenderer>(target, font_path_);
        runtime_.text_measure(renderer_->measure());   // carets land where the glyphs are
    } catch (const std::exception& e) {
        error_ = std::string("Tela GPU renderer could not start: ") + e.what();
    }
}

void TelaLayer::update(const render::FrameContext&) {
    if (!renderer_) return;
    ::tela::GpuQuad overlay{};
    if (reveal_.active()) {
        for (const auto& placed : runtime_.elements()) {
            if (placed.element.id != region_) continue;
            overlay.rect = ::tela::gpu_clip(::tela::intersect(placed.bounds, placed.clip), runtime_.viewport().dpi_scale);
            overlay.clip = overlay.rect;
            overlay.color = runtime_.theme().panel;
            overlay.color.a = reveal_.alpha();
            break;
        }
    }
    changed_ = renderer_->prepare(runtime_, overlay);
}

void TelaLayer::record(VkCommandBuffer cmd, VkExtent2D extent) {
    if (renderer_) renderer_->record(cmd, extent);
}

void TelaLayer::shutdown() {
    if (renderer_) runtime_.text_measure(::tela::approximate_text_width);
    renderer_.reset();
}

} // namespace ergo::studio::tela_shell
