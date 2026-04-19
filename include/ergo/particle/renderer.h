#pragma once

/// Vulkan billboard renderer for ergo_particle.
///
/// Compiles only when ERGO_PARTICLE_HAS_RENDERER is defined (i.e. the host
/// has both Pictor and Vulkan available). Reads particle instances out of
/// a ParticleSystem and records draw commands into a caller-supplied
/// command buffer.

#ifdef ERGO_PARTICLE_HAS_RENDERER

#include "ergo/particle/effect_config.h"
#include "ergo/particle/particle_system.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace pictor { class VulkanContext; }

namespace ergo::particle {

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// `shader_dir` is the directory containing `particle.vert.spv` and
    /// `particle.frag.spv` (typically `<exe_dir>/shaders`).
    bool initialize(pictor::VulkanContext& vk,
                    VkRenderPass           render_pass,
                    const std::string&     shader_dir,
                    uint32_t               max_particles = 4096);

    void shutdown();

    /// Sync GPU instance buffer from `system.instances()`. Call once per frame
    /// from the render thread, after `system.update(dt)`.
    void upload(const ParticleSystem& system);

    /// Record draw commands. `view`, `proj` are column-major float[16].
    /// `world_origin` is the world-space point that the particle local frame
    /// is anchored to (typically the player position).
    void record(VkCommandBuffer cmd,
                VkExtent2D      extent,
                const float     view[16],
                const float     proj[16],
                const float     world_origin[3],
                BlendMode       blend);

    bool is_initialized() const { return initialized_; }

private:
    bool create_quad_buffers();
    bool create_descriptor();
    bool create_pipeline(const std::string& shader_dir, BlendMode blend);
    VkPipeline pipeline_for(BlendMode b);
    uint32_t   find_memory_type(uint32_t filter, VkMemoryPropertyFlags props);
    bool       create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props,
                             VkBuffer& buf, VkDeviceMemory& mem);
    VkShaderModule load_shader(const std::string& path);

    pictor::VulkanContext* vk_         = nullptr;
    VkDevice               device_     = VK_NULL_HANDLE;
    VkRenderPass           render_pass_= VK_NULL_HANDLE;

    VkPipelineLayout       pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline             pipeline_add_    = VK_NULL_HANDLE;
    VkPipeline             pipeline_alpha_  = VK_NULL_HANDLE;
    VkDescriptorSetLayout  desc_layout_     = VK_NULL_HANDLE;
    VkDescriptorPool       desc_pool_       = VK_NULL_HANDLE;
    VkDescriptorSet        desc_set_        = VK_NULL_HANDLE;

    VkBuffer               quad_vb_     = VK_NULL_HANDLE;
    VkDeviceMemory         quad_vb_mem_ = VK_NULL_HANDLE;
    VkBuffer               quad_ib_     = VK_NULL_HANDLE;
    VkDeviceMemory         quad_ib_mem_ = VK_NULL_HANDLE;

    VkBuffer               instance_buf_     = VK_NULL_HANDLE;
    VkDeviceMemory         instance_mem_     = VK_NULL_HANDLE;
    uint32_t               max_particles_    = 0;
    uint32_t               instance_count_   = 0;

    VkBuffer               ubo_buf_     = VK_NULL_HANDLE;
    VkDeviceMemory         ubo_mem_     = VK_NULL_HANDLE;

    bool                   initialized_ = false;
};

} // namespace ergo::particle

#endif // ERGO_PARTICLE_HAS_RENDERER
