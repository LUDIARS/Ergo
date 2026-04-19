#include "ergo/particle/renderer.h"

#ifdef ERGO_PARTICLE_HAS_RENDERER

#include "pictor/surface/vulkan_context.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

namespace ergo::particle {

namespace {

struct VertexQuad {
    float pos[2];
    float uv[2];
};

struct InstanceGpu {
    float pos[3];
    float size;
    float color[4];
};

struct PushConsts {
    float world_origin[4]; // xyz + world_scale in w
    int   flags[4];        // x: shape_id (0 = circle, 1 = square)
};

struct SceneUbo {
    float view[16];
    float proj[16];
};

} // namespace

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(pictor::VulkanContext& vk,
                          VkRenderPass            render_pass,
                          const std::string&      shader_dir,
                          uint32_t                max_particles) {
    vk_            = &vk;
    device_        = vk.device();
    render_pass_   = render_pass;
    max_particles_ = max_particles;

    if (!create_quad_buffers()) return false;
    if (!create_descriptor())   return false;
    if (!create_pipeline(shader_dir, BlendMode::Additive)) return false;
    if (!create_pipeline(shader_dir, BlendMode::Alpha))    return false;

    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    if (!device_) return;
    vkDeviceWaitIdle(device_);
    if (pipeline_add_)    { vkDestroyPipeline(device_, pipeline_add_, nullptr); pipeline_add_ = VK_NULL_HANDLE; }
    if (pipeline_alpha_)  { vkDestroyPipeline(device_, pipeline_alpha_, nullptr); pipeline_alpha_ = VK_NULL_HANDLE; }
    if (pipeline_layout_) { vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr); pipeline_layout_ = VK_NULL_HANDLE; }
    if (desc_pool_)       { vkDestroyDescriptorPool(device_, desc_pool_, nullptr); desc_pool_ = VK_NULL_HANDLE; }
    if (desc_layout_)     { vkDestroyDescriptorSetLayout(device_, desc_layout_, nullptr); desc_layout_ = VK_NULL_HANDLE; }
    auto destroy_buf = [this](VkBuffer& b, VkDeviceMemory& m) {
        if (b) { vkDestroyBuffer(device_, b, nullptr); b = VK_NULL_HANDLE; }
        if (m) { vkFreeMemory(device_, m, nullptr);    m = VK_NULL_HANDLE; }
    };
    destroy_buf(instance_buf_, instance_mem_);
    destroy_buf(ubo_buf_,      ubo_mem_);
    destroy_buf(quad_vb_,      quad_vb_mem_);
    destroy_buf(quad_ib_,      quad_ib_mem_);
    initialized_ = false;
}

void Renderer::upload(const ParticleSystem& system) {
    if (!initialized_) return;
    const auto& src = system.instances();
    instance_count_ = static_cast<uint32_t>(src.size());
    if (instance_count_ > max_particles_) instance_count_ = max_particles_;
    if (instance_count_ == 0) return;

    void* mapped = nullptr;
    vkMapMemory(device_, instance_mem_, 0, sizeof(InstanceGpu) * instance_count_, 0, &mapped);
    InstanceGpu* dst = static_cast<InstanceGpu*>(mapped);
    for (uint32_t i = 0; i < instance_count_; ++i) {
        dst[i].pos[0]   = src[i].pos[0];
        dst[i].pos[1]   = src[i].pos[1];
        dst[i].pos[2]   = src[i].pos[2];
        dst[i].size     = src[i].size;
        dst[i].color[0] = src[i].color[0];
        dst[i].color[1] = src[i].color[1];
        dst[i].color[2] = src[i].color[2];
        dst[i].color[3] = src[i].color[3];
    }
    vkUnmapMemory(device_, instance_mem_);
}

VkPipeline Renderer::pipeline_for(BlendMode b) {
    return (b == BlendMode::Alpha) ? pipeline_alpha_ : pipeline_add_;
}

void Renderer::record(VkCommandBuffer cmd,
                      VkExtent2D       extent,
                      const float      view[16],
                      const float      proj[16],
                      const float      world_origin[3],
                      BlendMode        blend) {
    if (!initialized_ || instance_count_ == 0) return;

    SceneUbo ubo;
    std::memcpy(ubo.view, view, sizeof(ubo.view));
    std::memcpy(ubo.proj, proj, sizeof(ubo.proj));
    void* mapped = nullptr;
    vkMapMemory(device_, ubo_mem_, 0, sizeof(SceneUbo), 0, &mapped);
    std::memcpy(mapped, &ubo, sizeof(ubo));
    vkUnmapMemory(device_, ubo_mem_);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_for(blend));
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_, 0, 1, &desc_set_, 0, nullptr);

    VkViewport vp{};
    vp.width  = static_cast<float>(extent.width);
    vp.height = static_cast<float>(extent.height);
    vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D sc{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &sc);

    PushConsts pc{};
    pc.world_origin[0] = world_origin[0];
    pc.world_origin[1] = world_origin[1];
    pc.world_origin[2] = world_origin[2];
    pc.world_origin[3] = 0.02f; // world_scale: editor size 50 ~ 1 world unit
    pc.flags[0] = 0;            // shape: circle by default
    vkCmdPushConstants(cmd, pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    VkBuffer     vbufs[2]    = {quad_vb_, instance_buf_};
    VkDeviceSize voffsets[2] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, vbufs, voffsets);
    vkCmdBindIndexBuffer(cmd, quad_ib_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, instance_count_, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Buffer / memory helpers
// ---------------------------------------------------------------------------
uint32_t Renderer::find_memory_type(uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(vk_->physical_device(), &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    return UINT32_MAX;
}

bool Renderer::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props,
                             VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size  = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bi, nullptr, &buf) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, buf, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
    if (ai.memoryTypeIndex == UINT32_MAX) return false;
    if (vkAllocateMemory(device_, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindBufferMemory(device_, buf, mem, 0);
    return true;
}

VkShaderModule Renderer::load_shader(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open()) {
        fprintf(stderr, "[ergo::particle] cannot open shader %s\n", path.c_str());
        return VK_NULL_HANDLE;
    }
    size_t sz = static_cast<size_t>(f.tellg());
    std::vector<char> code(sz);
    f.seekg(0); f.read(code.data(), static_cast<std::streamsize>(sz));
    VkShaderModuleCreateInfo mi{};
    mi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mi.codeSize = sz;
    mi.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &mi, nullptr, &mod) != VK_SUCCESS) {
        fprintf(stderr, "[ergo::particle] vkCreateShaderModule failed for %s\n", path.c_str());
    }
    return mod;
}

// ---------------------------------------------------------------------------
// Quad mesh
// ---------------------------------------------------------------------------
bool Renderer::create_quad_buffers() {
    const VertexQuad verts[4] = {
        {{-0.5f, -0.5f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f}, {0.0f, 1.0f}},
    };
    const uint16_t idxs[6] = {0, 1, 2, 0, 2, 3};

    const VkMemoryPropertyFlags hv =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!create_buffer(sizeof(verts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hv,
                       quad_vb_, quad_vb_mem_)) return false;
    void* m = nullptr;
    vkMapMemory(device_, quad_vb_mem_, 0, sizeof(verts), 0, &m);
    std::memcpy(m, verts, sizeof(verts));
    vkUnmapMemory(device_, quad_vb_mem_);

    if (!create_buffer(sizeof(idxs), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hv,
                       quad_ib_, quad_ib_mem_)) return false;
    vkMapMemory(device_, quad_ib_mem_, 0, sizeof(idxs), 0, &m);
    std::memcpy(m, idxs, sizeof(idxs));
    vkUnmapMemory(device_, quad_ib_mem_);

    const VkDeviceSize isize = static_cast<VkDeviceSize>(max_particles_) * sizeof(InstanceGpu);
    if (!create_buffer(isize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hv,
                       instance_buf_, instance_mem_)) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Descriptor + UBO
// ---------------------------------------------------------------------------
bool Renderer::create_descriptor() {
    VkDescriptorSetLayoutBinding b{};
    b.binding         = 0;
    b.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo li{};
    li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings    = &b;
    if (vkCreateDescriptorSetLayout(device_, &li, nullptr, &desc_layout_) != VK_SUCCESS) return false;

    const VkMemoryPropertyFlags hv =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!create_buffer(sizeof(SceneUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hv,
                       ubo_buf_, ubo_mem_)) return false;

    VkDescriptorPoolSize ps{};
    ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pi{};
    pi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets       = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes    = &ps;
    if (vkCreateDescriptorPool(device_, &pi, nullptr, &desc_pool_) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = desc_pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &desc_layout_;
    if (vkAllocateDescriptorSets(device_, &ai, &desc_set_) != VK_SUCCESS) return false;

    VkDescriptorBufferInfo bi{};
    bi.buffer = ubo_buf_; bi.offset = 0; bi.range = sizeof(SceneUbo);
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = desc_set_; w.dstBinding = 0; w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo = &bi;
    vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
    return true;
}

// ---------------------------------------------------------------------------
// Pipeline
// ---------------------------------------------------------------------------
bool Renderer::create_pipeline(const std::string& shader_dir, BlendMode blend) {
    VkShaderModule vert = load_shader(shader_dir + "/particle.vert.spv");
    VkShaderModule frag = load_shader(shader_dir + "/particle.frag.spv");
    if (!vert || !frag) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert; stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag; stages[1].pName = "main";

    VkVertexInputBindingDescription bindings[2]{};
    bindings[0].binding   = 0;
    bindings[0].stride    = sizeof(VertexQuad);
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings[1].binding   = 1;
    bindings[1].stride    = sizeof(InstanceGpu);
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription attrs[5]{};
    // per-vertex
    attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexQuad, pos)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexQuad, uv)};
    // per-instance
    attrs[2] = {2, 1, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(InstanceGpu, pos)};
    attrs[3] = {3, 1, VK_FORMAT_R32_SFLOAT,          offsetof(InstanceGpu, size)};
    attrs[4] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceGpu, color)};

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = 2;
    vi.pVertexBindingDescriptions      = bindings;
    vi.vertexAttributeDescriptionCount = 5;
    vi.pVertexAttributeDescriptions    = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1; vs.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_NONE;        // billboards are double-sided
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // No depth in the host's render pass currently; safe to disable.
    ds.depthTestEnable  = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ba.blendEnable = VK_TRUE;
    if (blend == BlendMode::Additive) {
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.colorBlendOp        = VK_BLEND_OP_ADD;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.alphaBlendOp        = VK_BLEND_OP_ADD;
    } else { // Alpha
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.colorBlendOp        = VK_BLEND_OP_ADD;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &ba;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{};
    dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount = 2;
    dy.pDynamicStates    = dyn;

    if (pipeline_layout_ == VK_NULL_HANDLE) {
        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(PushConsts);

        VkPipelineLayoutCreateInfo pli{};
        pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount = 1;
        pli.pSetLayouts = &desc_layout_;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges = &pcr;
        if (vkCreatePipelineLayout(device_, &pli, nullptr, &pipeline_layout_) != VK_SUCCESS) {
            vkDestroyShaderModule(device_, vert, nullptr);
            vkDestroyShaderModule(device_, frag, nullptr);
            return false;
        }
    }

    VkGraphicsPipelineCreateInfo gpi{};
    gpi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpi.stageCount = 2;
    gpi.pStages = stages;
    gpi.pVertexInputState   = &vi;
    gpi.pInputAssemblyState = &ia;
    gpi.pViewportState      = &vs;
    gpi.pRasterizationState = &rs;
    gpi.pMultisampleState   = &ms;
    gpi.pDepthStencilState  = &ds;
    gpi.pColorBlendState    = &cb;
    gpi.pDynamicState       = &dy;
    gpi.layout              = pipeline_layout_;
    gpi.renderPass          = render_pass_;
    gpi.subpass             = 0;

    VkPipeline* slot = (blend == BlendMode::Alpha) ? &pipeline_alpha_ : &pipeline_add_;
    VkResult r = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gpi, nullptr, slot);

    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "[ergo::particle] vkCreateGraphicsPipelines failed (%d)\n", r);
        return false;
    }
    return true;
}

} // namespace ergo::particle

#endif // ERGO_PARTICLE_HAS_RENDERER
