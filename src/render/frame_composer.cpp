#include "ergo/render/frame_composer.h"

#include "ergo/render/render_context.h"

#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
#include "pictor/surface/vulkan_context.h"
#endif

#include <cstdio>

namespace ergo::render {

FrameComposer::~FrameComposer() {
    shutdown();
}

void FrameComposer::add_pass(VkRenderPass render_pass,
                             std::vector<IRenderLayer*> layers) {
    // clear_values 省略版。 color 1 枚を黒クリアする既定値を入れる。
    std::vector<VkClearValue> clears;
#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
    VkClearValue c{};
    c.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clears.push_back(c);
#endif
    add_pass(render_pass, std::move(layers), std::move(clears));
}

void FrameComposer::add_pass(VkRenderPass render_pass,
                             std::vector<IRenderLayer*> layers,
                             std::vector<VkClearValue>  clear_values) {
    RenderPassDesc desc;
    desc.render_pass  = render_pass;
    desc.layers       = std::move(layers);
    desc.clear_values = std::move(clear_values);
    passes_.push_back(std::move(desc));
    fb_providers_.emplace_back();   // パス数と 1:1 で provider スロットを増やす
    pre_pass_hooks_.emplace_back(); // パス数と 1:1 で pre-pass hook スロットを増やす
}

void FrameComposer::set_framebuffer_provider(
        std::size_t pass_index,
        VkFramebuffer (*provider)(uint32_t, void*),
        void* user) {
    if (pass_index >= fb_providers_.size()) return;
    fb_providers_[pass_index].fn   = provider;
    fb_providers_[pass_index].user = user;
}

void FrameComposer::set_pre_pass_hook(
        std::size_t pass_index,
        void (*hook)(VkCommandBuffer, uint32_t, const FrameContext&, void*),
        void* user) {
    if (pass_index >= pre_pass_hooks_.size()) return;
    pre_pass_hooks_[pass_index].fn   = hook;
    pre_pass_hooks_[pass_index].user = user;
}

void FrameComposer::set_post_present_hook(
        void (*hook)(uint32_t, const FrameContext&, void*),
        void* user) {
    post_present_hook_.fn   = hook;
    post_present_hook_.user = user;
}

void FrameComposer::initialize(RenderContext& ctx) {
    if (initialized_) return;
    ctx_ = &ctx;

    // 1. 全パス × 全レイヤーを登録順に initialize()。
    //    add_pass の呼び出し順 × パス内レイヤー順 = 初期化順。
    for (auto& pass : passes_) {
        for (IRenderLayer* layer : pass.layers) {
            if (layer) layer->initialize(ctx);
        }
    }
    // 2. 各レイヤーへ所属パスの render pass を通知する。 pipeline 構築は
    //    render pass に依存するため initialize() とは別ステップにする。
    for (auto& pass : passes_) {
        for (IRenderLayer* layer : pass.layers) {
            if (layer) layer->set_render_pass(pass.render_pass);
        }
    }

    initialized_   = true;
    first_frame_   = true;
    shutdown_done_ = false;
}

bool FrameComposer::run_frame(const FrameContext& frame) {
#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
    if (!initialized_ || !ctx_ || !ctx_->vk) return false;
    pictor::VulkanContext* vk = ctx_->vk;

    // 1. swapchain image を取得する。 fence 待ち / リセットは VulkanContext が
    //    内部で行う。 out-of-date のときは UINT32_MAX が返り swapchain は
    //    再生成済み — このフレームはスキップして継続する。
    const uint32_t image_idx = vk->acquire_next_image();
    if (image_idx == UINT32_MAX) return true;

    // 2. 初回フレームのみ on_first_frame() を回す。 .riv の初回 bake のような
    //    重い初期化は「最初のフレームを提示する前」ではなく、 ここ (acquire 後)
    //    に置くことで起動時フリーズを最小化する。
    if (first_frame_) {
        for (auto& pass : passes_) {
            for (IRenderLayer* layer : pass.layers) {
                if (layer) layer->on_first_frame(*ctx_);
            }
        }
        first_frame_ = false;
    }

    // 3. 全レイヤー update() — drawlist 構築 / UBO 計算など。
    for (auto& pass : passes_) {
        for (IRenderLayer* layer : pass.layers) {
            if (layer) layer->update(frame);
        }
    }

    // 4. コマンドバッファ記録。 パスごとに begin → 各レイヤー record → end。
    VkCommandBuffer cmd = vk->command_buffers()[image_idx];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo cbi{};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &cbi) != VK_SUCCESS) return false;

    const VkExtent2D extent = vk->swapchain_extent();

    for (std::size_t i = 0; i < passes_.size(); ++i) {
        RenderPassDesc& pass = passes_[i];

        // パス間 hook: このパスを begin する直前、 render pass の外側で
        // 自前合成 (post-process チェーン / 投影デカール) を記録する。
        if (i < pre_pass_hooks_.size() && pre_pass_hooks_[i].fn) {
            pre_pass_hooks_[i].fn(cmd, image_idx, frame, pre_pass_hooks_[i].user);
        }

        // framebuffer の解決: provider が登録されていればそれを使い、
        // 無ければ VulkanContext の default framebuffers から image_idx 番。
        VkFramebuffer fb = VK_NULL_HANDLE;
        if (fb_providers_[i].fn) {
            fb = fb_providers_[i].fn(image_idx, fb_providers_[i].user);
        } else {
            const auto& fbs = vk->framebuffers();
            if (image_idx < fbs.size()) fb = fbs[image_idx];
        }

        // render pass / framebuffer が解決できないパスは安全に飛ばす。
        if (pass.render_pass == VK_NULL_HANDLE || fb == VK_NULL_HANDLE) {
            continue;
        }

        VkRenderPassBeginInfo rp{};
        rp.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass      = pass.render_pass;
        rp.framebuffer     = fb;
        rp.renderArea      = {{0, 0}, extent};
        rp.clearValueCount = static_cast<uint32_t>(pass.clear_values.size());
        rp.pClearValues    = pass.clear_values.empty()
                               ? nullptr : pass.clear_values.data();
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        for (IRenderLayer* layer : pass.layers) {
            if (layer) layer->record(cmd, extent);
        }
        vkCmdEndRenderPass(cmd);
    }

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) return false;

    // 5. submit + present。 swapchain 取得待ちのセマフォを wait、 描画完了の
    //    セマフォを signal し、 in-flight fence を立てる (次フレームの
    //    acquire_next_image がこの fence を待つ)。
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphore wait_sem = vk->image_available_semaphore();
    VkSemaphore sig_sem  = vk->render_finished_semaphore();
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &wait_sem;
    si.pWaitDstStageMask    = &wait_stage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &sig_sem;
    if (vkQueueSubmit(vk->graphics_queue(), 1, &si, vk->in_flight_fence())
            != VK_SUCCESS) {
        return false;
    }

    vk->present(image_idx);
    last_present_idx_ = image_idx;
    ++frame_count_;

    // present 直後の hook: 提示済み swapchain image の読み出し
    // (スクリーンショット) など、 「提示後に 1 回」やる処理を回す。
    if (post_present_hook_.fn) {
        post_present_hook_.fn(image_idx, frame, post_present_hook_.user);
    }
    return true;
#else
    // Vulkan SDK 無しビルド — フレームループは no-op。 ホストのループが
    // 空回りしないよう false (継続不能) を返す。
    (void)frame;
    return false;
#endif
}

void FrameComposer::shutdown() {
    if (shutdown_done_ || !initialized_) {
        shutdown_done_ = true;
        return;
    }
    // 登録の逆順に shutdown()。 パスを後ろから、 パス内レイヤーも後ろから。
    for (auto it = passes_.rbegin(); it != passes_.rend(); ++it) {
        for (auto lit = it->layers.rbegin(); lit != it->layers.rend(); ++lit) {
            if (*lit) (*lit)->shutdown();
        }
    }
    shutdown_done_ = true;
    initialized_   = false;
}

} // namespace ergo::render
