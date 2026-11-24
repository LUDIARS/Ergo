#include "preview_gpu.h"

#include "asset_catalog.h"
#include "png_writer.h"
#include "pictor/shader/graphics_pipeline_builder.h"

#include <lorica/checked_size.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {
namespace {

void check(VkResult result, const char* error) {
    if (result != VK_SUCCESS) throw std::runtime_error(error);
}

uint32_t memory_type(pictor::VulkanContext& vk, uint32_t mask, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(vk.physical_device(), &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((mask & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) return i;
    }
    throw std::runtime_error("no suitable memory for Visus preview");
}

struct Buffer {
    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    explicit Buffer(VkDevice owner) : device(owner) {}
    ~Buffer() {
        if (mapped) vkUnmapMemory(device, memory);
        if (buffer) vkDestroyBuffer(device, buffer, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
    }

    void create(pictor::VulkanContext& vk, VkDeviceSize bytes, VkBufferUsageFlags usage) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = bytes;
        info.usage = usage;
        check(vkCreateBuffer(device, &info, nullptr, &buffer), "could not create Visus preview buffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_type(vk, requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkAllocateMemory(device, &allocation, nullptr, &memory), "could not allocate Visus preview buffer");
        check(vkBindBufferMemory(device, buffer, memory, 0), "could not bind Visus preview buffer");
        check(vkMapMemory(device, memory, 0, bytes, 0, &mapped), "could not map Visus preview buffer");
    }
};

struct ScenePass {
    VkDevice device;
    VkImage depth = VK_NULL_HANDLE;
    VkDeviceMemory depth_memory = VK_NULL_HANDLE;
    VkImageView depth_view = VK_NULL_HANDLE;
    VkRenderPass pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkShaderModule vert = VK_NULL_HANDLE;
    VkShaderModule frag = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    explicit ScenePass(VkDevice owner) : device(owner) {}
    ~ScenePass() {
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        if (layout) vkDestroyPipelineLayout(device, layout, nullptr);
        if (vert) vkDestroyShaderModule(device, vert, nullptr);
        if (frag) vkDestroyShaderModule(device, frag, nullptr);
        if (framebuffer) vkDestroyFramebuffer(device, framebuffer, nullptr);
        if (pass) vkDestroyRenderPass(device, pass, nullptr);
        if (depth_view) vkDestroyImageView(device, depth_view, nullptr);
        if (depth) vkDestroyImage(device, depth, nullptr);
        if (depth_memory) vkFreeMemory(device, depth_memory, nullptr);
    }

    void create_depth(pictor::VulkanContext& vk, VkExtent2D extent) {
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.imageType = VK_IMAGE_TYPE_2D;
        image.format = VK_FORMAT_D16_UNORM;
        image.extent = {extent.width, extent.height, 1};
        image.mipLevels = image.arrayLayers = 1;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        check(vkCreateImage(device, &image, nullptr, &depth), "could not create preview depth image");
        VkMemoryRequirements needed{};
        vkGetImageMemoryRequirements(device, depth, &needed);
        VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memory.allocationSize = needed.size;
        memory.memoryTypeIndex = memory_type(vk, needed.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(device, &memory, nullptr, &depth_memory), "could not allocate preview depth memory");
        check(vkBindImageMemory(device, depth, depth_memory, 0), "could not bind preview depth memory");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = depth;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = image.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(device, &view, nullptr, &depth_view), "could not create preview depth view");
    }

    void create_pass(pictor::VulkanContext& vk, uint32_t index, VkExtent2D extent) {
        VkAttachmentDescription attachments[2]{};
        for (auto& a : attachments) {
            a.samples = VK_SAMPLE_COUNT_1_BIT;
            a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            a.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
        attachments[0].format = vk.swapchain_format();
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[1].format = VK_FORMAT_D16_UNORM;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        const VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color;
        subpass.pDepthStencilAttachment = &depth_ref;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = dependency.srcStageMask;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        check(vkCreateRenderPass(device, &info, nullptr, &pass), "could not create preview render pass");
        const VkImageView views[] = {vk.swapchain_image_views().at(index), depth_view};
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass = pass;
        fb.attachmentCount = 2;
        fb.pAttachments = views;
        fb.width = extent.width;
        fb.height = extent.height;
        fb.layers = 1;
        check(vkCreateFramebuffer(device, &fb, nullptr, &framebuffer), "could not create preview framebuffer");
    }

    void load_shader(const std::filesystem::path& path, VkShaderModule& module) {
        const auto bytes = read_bounded(path, 256 * 1024);
        if (!bytes || bytes->empty() || bytes->size() % sizeof(uint32_t)) {
            throw std::runtime_error("invalid preview shader binary");
        }
        std::vector<uint32_t> words(bytes->size() / sizeof(uint32_t));
        std::memcpy(words.data(), bytes->data(), bytes->size());
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = bytes->size();
        info.pCode = words.data();
        check(vkCreateShaderModule(device, &info, nullptr, &module), "could not load preview shader");
    }

    void create_pipeline(const std::filesystem::path& shaders) {
        load_shader(shaders / "asset_preview.vert.spv", vert);
        load_shader(shaders / "asset_preview.frag.spv", frag);
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        check(vkCreatePipelineLayout(device, &info, nullptr, &layout), "could not create preview pipeline layout");
        pictor::GraphicsPipelineDesc desc;
        desc.vert = vert;
        desc.frag = frag;
        desc.render_pass = pass;
        desc.layout = layout;
        desc.depth_test = true;
        desc.vertex_layout.stride = sizeof(PreviewVertex);
        desc.vertex_layout.attributes = {
            {pictor::VertexSemantic::POSITION, pictor::VertexAttributeType::FLOAT3, offsetof(PreviewVertex, position)},
            {pictor::VertexSemantic::COLOR0, pictor::VertexAttributeType::FLOAT3, offsetof(PreviewVertex, color)},
        };
        if (!pictor::build_graphics_pipeline(device, desc, pipeline)) throw std::runtime_error("could not create preview pipeline");
    }
};

void copy_frame(VkCommandBuffer cmd, VkImage image, VkBuffer target, VkExtent2D extent) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target, 1, &region);
    std::swap(barrier.oldLayout, barrier.newLayout);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

std::string thumbnail(const uint8_t* source, VkExtent2D extent, VkFormat format) {
    constexpr std::size_t side = 192;
    const bool bgra = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM;
    if (!bgra && format != VK_FORMAT_R8G8B8A8_SRGB && format != VK_FORMAT_R8G8B8A8_UNORM) {
        throw std::runtime_error("unsupported Visus capture pixel format");
    }
    std::vector<uint8_t> rgba(side * side * 4);
    for (std::size_t y = 0; y < side; ++y) for (std::size_t x = 0; x < side; ++x) {
        const auto* pixel = source + (y * extent.height / side * extent.width + x * extent.width / side) * 4;
        auto* dst = rgba.data() + (y * side + x) * 4;
        dst[0] = pixel[bgra ? 2 : 0];
        dst[1] = pixel[1];
        dst[2] = pixel[bgra ? 0 : 2];
        dst[3] = 255;
    }
    const auto encoded = ergo::custos::detail::encode_png_rgba8(rgba.data(), side, side);
    return {encoded.begin(), encoded.end()};
}

} // namespace

std::string draw_preview(pictor::VulkanContext& vk, const std::filesystem::path& shaders,
                         const std::vector<PreviewVertex>& vertices, bool capture) {
    auto image = vk.acquire_frame();
    if (!image.has_image() && image.swapchain_recreated) image = vk.acquire_frame();
    if (!image.has_image()) throw std::runtime_error("could not acquire preview swapchain image");
    const auto index = image.image_index;
    const auto extent = vk.swapchain_extent();
    const auto format = vk.swapchain_format();
    if (!extent.width || !extent.height || extent.width > 4096 || extent.height > 4096) {
        throw std::runtime_error("Visus preview framebuffer exceeds size limit");
    }
    const auto bytes = lorica::checked_multiply(vertices.size(), sizeof(PreviewVertex));
    if (!bytes || !*bytes) throw std::runtime_error("invalid preview mesh size");
    Buffer mesh(vk.device());
    mesh.create(vk, *bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    std::memcpy(mesh.mapped, vertices.data(), *bytes);
    Buffer readback(vk.device());
    if (capture) readback.create(vk, VkDeviceSize(extent.width) * extent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    ScenePass scene(vk.device());
    scene.create_depth(vk, extent);
    scene.create_pass(vk, index, extent);
    scene.create_pipeline(shaders);
    const auto cmd = vk.command_buffers().at(index);
    check(vkResetCommandBuffer(cmd, 0), "could not reset preview command buffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(cmd, &begin), "could not begin preview commands");
    VkClearValue clear[2]{};
    clear[0].color = {{.025f, .032f, .046f, 1.f}};
    clear[1].depthStencil.depth = 1.f;
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = scene.pass;
    pass.framebuffer = scene.framebuffer;
    pass.renderArea = {{0, 0}, extent};
    pass.clearValueCount = 2;
    pass.pClearValues = clear;
    vkCmdBeginRenderPass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);
    const auto side = static_cast<float>(std::min(extent.width, extent.height));
    const VkViewport viewport{(extent.width - side) / 2, (extent.height - side) / 2, side, side, 0, 1};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &pass.renderArea);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scene.pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.buffer, &offset);
    vkCmdDraw(cmd, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    if (capture) copy_frame(cmd, vk.swapchain_images().at(index), readback.buffer, extent);
    check(vkEndCommandBuffer(cmd), "could not end preview commands");
    const auto ready = vk.image_available_semaphore();
    const auto rendered = vk.render_finished_semaphore();
    const auto fence = vk.in_flight_fence();
    const VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &ready;
    submit.pWaitDstStageMask = &wait;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &rendered;
    check(vkResetFences(vk.device(), 1, &fence), "could not reset preview fence");
    check(vkQueueSubmit(vk.graphics_queue(), 1, &submit, fence), "could not submit preview commands");
    const auto finished = vkWaitForFences(vk.device(), 1, &fence, VK_TRUE, UINT64_MAX);
    if (finished != VK_SUCCESS) vk.device_wait_idle();
    check(finished, "Visus preview did not finish");
    // Release the temporary framebuffer before present can recreate the swapchain.
    vkDestroyFramebuffer(vk.device(), scene.framebuffer, nullptr);
    scene.framebuffer = VK_NULL_HANDLE;
    const auto present = vk.present_frame(index);
    if (present.status != pictor::FrameStatus::Ready && !present.swapchain_recreated) {
        throw std::runtime_error("could not present Visus preview");
    }
    return capture ? thumbnail(static_cast<const uint8_t*>(readback.mapped), extent, format) : std::string{};
}

} // namespace ergo::studio::plugins::asset_import
