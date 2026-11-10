#include "ergo/studio/shell/ui_layer.h"

#include "ergo/render/render_context.h"
#include "pictor/surface/vulkan_context.h"
#include "pictor/ui/ui_renderer.h"

#include <cmath>
#include <cstring>

namespace ergo::studio::shell {
namespace {

/// The swapchain is *_SRGB: the hardware encodes on write, so colours given in
/// sRGB (the theme, design tools) must be linearised first or they wash out.
float srgb_to_linear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

} // namespace

/// Vulkan objects of the overlay texture.
struct UiLayer::Impl {
    pictor::VulkanContext* vk = nullptr;
    pictor::UIRenderer     renderer;
    TextCanvas             canvas;
    uint32_t               ow = 0, oh = 0;
    VkImage                image   = VK_NULL_HANDLE;
    VkDeviceMemory         memory  = VK_NULL_HANDLE;
    VkImageView            view    = VK_NULL_HANDLE;
    VkBuffer               staging = VK_NULL_HANDLE;
    VkDeviceMemory         staging_mem = VK_NULL_HANDLE;
    void*                  mapped  = nullptr;
    uint32_t               texture = 0;
    bool                   first_upload = true;
    VkRenderPass           pass = VK_NULL_HANDLE;

    uint32_t memory_type(uint32_t filter, VkMemoryPropertyFlags props) const {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(vk->physical_device(), &mp);
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            if ((filter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
        }
        return UINT32_MAX;
    }

    bool create(std::string& err) {
        VkDevice dev = vk->device();
        VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType     = VK_IMAGE_TYPE_2D;
        ii.format        = VK_FORMAT_R8G8B8A8_SRGB;   // canvas bytes are sRGB; sampling linearises them
        ii.extent        = {ow, oh, 1};
        ii.mipLevels     = 1;
        ii.arrayLayers   = 1;
        ii.samples       = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ii.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(dev, &ii, nullptr, &image) != VK_SUCCESS) { err = "vkCreateImage failed"; return false; }
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(dev, image, &req);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(dev, &ai, nullptr, &memory) != VK_SUCCESS) {
            err = "overlay image memory";
            return false;
        }
        vkBindImageMemory(dev, image, memory, 0);

        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image            = image;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R8G8B8A8_SRGB;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(dev, &vi, nullptr, &view) != VK_SUCCESS) { err = "vkCreateImageView failed"; return false; }

        const VkDeviceSize bytes = static_cast<VkDeviceSize>(ow) * oh * 4;
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size  = bytes;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (vkCreateBuffer(dev, &bi, nullptr, &staging) != VK_SUCCESS) { err = "staging buffer"; return false; }
        vkGetBufferMemoryRequirements(dev, staging, &req);
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = memory_type(req.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(dev, &ai, nullptr, &staging_mem) != VK_SUCCESS) {
            err = "staging memory";
            return false;
        }
        vkBindBufferMemory(dev, staging, staging_mem, 0);
        vkMapMemory(dev, staging_mem, 0, bytes, 0, &mapped);
        return true;
    }

    void barrier(VkCommandBuffer cmd, VkImageLayout from, VkImageLayout to, VkAccessFlags src, VkAccessFlags dst,
                 VkPipelineStageFlags s0, VkPipelineStageFlags s1) const {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout           = from;
        b.newLayout           = to;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image               = image;
        b.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.srcAccessMask       = src;
        b.dstAccessMask       = dst;
        vkCmdPipelineBarrier(cmd, s0, s1, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    void upload() {
        std::memcpy(mapped, canvas.pixels().data(), canvas.pixels().size());
        VkCommandBuffer cmd = vk->begin_single_time_commands();
        barrier(cmd, first_upload ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, first_upload ? 0 : VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, first_upload ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent      = {ow, oh, 1};
        vkCmdCopyBufferToImage(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        vk->end_single_time_commands(cmd);   // waits: the frame's draw never samples a half copy
        first_upload = false;
    }

    void destroy() {
        if (!vk) return;
        VkDevice dev = vk->device();
        vk->device_wait_idle();
        renderer.shutdown();
        if (mapped) vkUnmapMemory(dev, staging_mem);
        if (staging) vkDestroyBuffer(dev, staging, nullptr);
        if (staging_mem) vkFreeMemory(dev, staging_mem, nullptr);
        if (view) vkDestroyImageView(dev, view, nullptr);
        if (image) vkDestroyImage(dev, image, nullptr);
        if (memory) vkFreeMemory(dev, memory, nullptr);
        mapped = nullptr;
        staging = VK_NULL_HANDLE;
        staging_mem = memory = VK_NULL_HANDLE;
        view = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        vk = nullptr;
    }
};

UiLayer::UiLayer(const FontAtlas& font, uint32_t overlay_w, uint32_t overlay_h, std::string shader_dir)
    : impl_(std::make_unique<Impl>()), font_(font), shader_dir_(std::move(shader_dir)) {
    impl_->ow = overlay_w;
    impl_->oh = overlay_h;
}

UiLayer::~UiLayer() { shutdown(); }

void UiLayer::initialize(render::RenderContext& ctx) {
    impl_->vk = ctx.vk;
    if (!impl_->vk) {
        error_ = "no Vulkan context";
        return;
    }
    std::string err;
    if (!impl_->create(err)) error_ = err;
    impl_->canvas.resize(impl_->ow, impl_->oh);
}

void UiLayer::set_render_pass(VkRenderPass render_pass) {
    impl_->pass = render_pass;
    if (!error_.empty() || !impl_->vk) return;
    if (!impl_->renderer.initialize(*impl_->vk, shader_dir_, render_pass)) {
        error_ = "UIRenderer could not start (shaders in " + shader_dir_ + "?)";
        return;
    }
    impl_->texture = impl_->renderer.register_texture(impl_->view);
}

void UiLayer::update(const render::FrameContext&) {
    if (!error_.empty()) return;
    if (impl_->canvas.render(list_, font_)) impl_->upload();
}

void UiLayer::record(VkCommandBuffer cmd, VkExtent2D extent) {
    if (!error_.empty()) return;
    std::vector<pictor::UIDrawCmd> out;
    out.reserve(list_.size() + 1);
    for (const auto& c : list_) {
        pictor::UIDrawCmd d;
        switch (c.kind) {
            case ui_kit::DrawKind::Rect: d.kind = pictor::UIDrawKind::Rect; break;
            case ui_kit::DrawKind::NineSlice: d.kind = pictor::UIDrawKind::NineSlice; break;
            case ui_kit::DrawKind::Image: d.kind = pictor::UIDrawKind::Image; break;
            case ui_kit::DrawKind::PushClip: d.kind = pictor::UIDrawKind::PushClip; break;
            case ui_kit::DrawKind::PopClip: d.kind = pictor::UIDrawKind::PopClip; break;
            case ui_kit::DrawKind::Text: continue;   // all text is in the overlay
        }
        d.x = c.rect.x;
        d.y = c.rect.y;
        d.w = c.rect.w;
        d.h = c.rect.h;
        d.r = srgb_to_linear(c.color.r);
        d.g = srgb_to_linear(c.color.g);
        d.b = srgb_to_linear(c.color.b);
        d.a = c.color.a;
        d.corner_radius = c.corner_radius;
        d.texture_id    = c.texture_id;
        std::memcpy(d.nine, c.nine, sizeof(d.nine));
        d.opacity = c.opacity;
        out.push_back(d);
    }
    pictor::UIDrawCmd overlay;
    overlay.kind       = pictor::UIDrawKind::Image;
    overlay.w          = static_cast<float>(impl_->ow);
    overlay.h          = static_cast<float>(impl_->oh);
    overlay.texture_id = impl_->texture;
    out.push_back(overlay);
    impl_->renderer.record(cmd, extent, out);
}

void UiLayer::shutdown() {
    if (impl_) impl_->destroy();
}

} // namespace ergo::studio::shell
