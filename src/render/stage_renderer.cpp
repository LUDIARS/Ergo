#include "ergo/render/stage_renderer.h"

#include "ergo/render/render_context.h"

#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
#include "pictor/surface/vulkan_context.h"
#endif

#include <cstdio>
#include <cstring>
#include <fstream>

namespace ergo::render {

void generate_unit_cube(std::vector<StageVertex>& verts,
                        std::vector<uint32_t>&    idxs) {
    constexpr float P =  0.5f;
    constexpr float N = -0.5f;
    struct Face { float x, y, z, nx, ny, nz; };
    const Face data[24] = {
        {N,N,P, 0,0, 1}, {P,N,P, 0,0, 1}, {P,P,P, 0,0, 1}, {N,P,P, 0,0, 1},
        {P,N,N, 0,0,-1}, {N,N,N, 0,0,-1}, {N,P,N, 0,0,-1}, {P,P,N, 0,0,-1},
        {P,N,P, 1,0, 0}, {P,N,N, 1,0, 0}, {P,P,N, 1,0, 0}, {P,P,P, 1,0, 0},
        {N,N,N,-1,0, 0}, {N,N,P,-1,0, 0}, {N,P,P,-1,0, 0}, {N,P,N,-1,0, 0},
        {N,P,P, 0, 1,0}, {P,P,P, 0, 1,0}, {P,P,N, 0, 1,0}, {N,P,N, 0, 1,0},
        {N,N,N, 0,-1,0}, {P,N,N, 0,-1,0}, {P,N,P, 0,-1,0}, {N,N,P, 0,-1,0},
    };
    verts.clear();
    idxs.clear();
    verts.reserve(24);
    for (int i = 0; i < 24; ++i) {
        verts.push_back({{data[i].x, data[i].y, data[i].z},
                         {data[i].nx, data[i].ny, data[i].nz}});
    }
    idxs.reserve(36);
    for (uint32_t f = 0; f < 6; ++f) {
        uint32_t b = f * 4;
        idxs.push_back(b + 0); idxs.push_back(b + 1); idxs.push_back(b + 2);
        idxs.push_back(b + 0); idxs.push_back(b + 2); idxs.push_back(b + 3);
    }
}

void StageRenderer::set_light_dir(const float light_dir[4]) {
    std::memcpy(light_dir_, light_dir, sizeof(light_dir_));
}

void StageRenderer::set_drawables(const StageDrawable* drawables,
                                  uint32_t count) {
    drawables_.assign(drawables, drawables + count);
}

void StageRenderer::set_drawables(const std::vector<StageDrawable>& drawables) {
    drawables_ = drawables;
}

#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)

namespace {
struct SceneUBO {
    float view[16];
    float proj[16];
    float lightDir[4];
};
struct PushConsts {
    float model[16];
    float baseColor[4];
};
} // namespace

StageRenderer::~StageRenderer() { shutdown(); }

void StageRenderer::set_render_pass(VkRenderPass render_pass) {
    render_pass_ = render_pass;
}

void StageRenderer::initialize(RenderContext& ctx) {
    if (initialized_) return;
    if (!ctx.vk) {
        std::fprintf(stderr, "[StageRenderer] RenderContext.vk is null\n");
        return;
    }
    vk_     = ctx.vk;
    device_ = ctx.vk->device();
    if (!create_descriptor_set_()) return;
    if (!create_pipeline_(ctx.shader_dir)) return;
    initialized_ = true;
}

void StageRenderer::shutdown() {
    if (!initialized_) return;
    vkDeviceWaitIdle(device_);
    for (auto& m : owned_meshes_) destroy_mesh_(m);
    owned_meshes_.clear();
    if (pipeline_)        vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipeline_layout_) vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    if (desc_pool_)       vkDestroyDescriptorPool(device_, desc_pool_, nullptr);
    if (desc_layout_)     vkDestroyDescriptorSetLayout(device_, desc_layout_, nullptr);
    if (ubo_buf_)         vkDestroyBuffer(device_, ubo_buf_, nullptr);
    if (ubo_mem_)         vkFreeMemory(device_, ubo_mem_, nullptr);
    pipeline_        = VK_NULL_HANDLE;
    pipeline_layout_ = VK_NULL_HANDLE;
    desc_pool_       = VK_NULL_HANDLE;
    desc_layout_     = VK_NULL_HANDLE;
    ubo_buf_         = VK_NULL_HANDLE;
    ubo_mem_         = VK_NULL_HANDLE;
    initialized_     = false;
}

StageMesh* StageRenderer::upload_mesh(const std::vector<StageVertex>& verts,
                                      const std::vector<uint32_t>&    idxs) {
    if (!vk_) return nullptr;
    StageMesh m;
    m.index_count = static_cast<uint32_t>(idxs.size());
    const VkMemoryPropertyFlags hv =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkDeviceSize vb_size = verts.size() * sizeof(StageVertex);
    const VkDeviceSize ib_size = idxs.size()  * sizeof(uint32_t);

    if (!create_buffer_(vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hv,
                        m.vb, m.vb_mem)) return nullptr;
    if (!create_buffer_(ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hv,
                        m.ib, m.ib_mem)) return nullptr;

    void* mapped = nullptr;
    vkMapMemory(device_, m.vb_mem, 0, vb_size, 0, &mapped);
    std::memcpy(mapped, verts.data(), vb_size);
    vkUnmapMemory(device_, m.vb_mem);
    vkMapMemory(device_, m.ib_mem, 0, ib_size, 0, &mapped);
    std::memcpy(mapped, idxs.data(), ib_size);
    vkUnmapMemory(device_, m.ib_mem);

    owned_meshes_.push_back(m);
    return &owned_meshes_.back();
}

void StageRenderer::update(const FrameContext& frame) {
    if (!initialized_) return;
    SceneUBO ubo{};
    std::memcpy(ubo.view,     frame.view, sizeof(ubo.view));
    std::memcpy(ubo.proj,     frame.proj, sizeof(ubo.proj));
    std::memcpy(ubo.lightDir, light_dir_, sizeof(ubo.lightDir));
    void* mapped = nullptr;
    vkMapMemory(device_, ubo_mem_, 0, sizeof(ubo), 0, &mapped);
    std::memcpy(mapped, &ubo, sizeof(ubo));
    vkUnmapMemory(device_, ubo_mem_);
}

void StageRenderer::record(VkCommandBuffer cmd, VkExtent2D extent) {
    if (!initialized_) return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_, 0, 1, &desc_set_, 0, nullptr);

    VkViewport vp{};
    vp.width    = static_cast<float>(extent.width);
    vp.height   = static_cast<float>(extent.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D sc{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &sc);

    for (const auto& d : drawables_) {
        if (!d.mesh || d.mesh->index_count == 0) continue;
        PushConsts pc{};
        std::memcpy(pc.model,     d.model, sizeof(pc.model));
        std::memcpy(pc.baseColor, d.color, sizeof(pc.baseColor));
        vkCmdPushConstants(cmd, pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &d.mesh->vb, &offset);
        vkCmdBindIndexBuffer(cmd, d.mesh->ib, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, d.mesh->index_count, 1, 0, 0, 0);
    }
}

void StageRenderer::destroy_mesh_(StageMesh& m) {
    if (m.vb)     vkDestroyBuffer(device_, m.vb, nullptr);
    if (m.ib)     vkDestroyBuffer(device_, m.ib, nullptr);
    if (m.vb_mem) vkFreeMemory(device_, m.vb_mem, nullptr);
    if (m.ib_mem) vkFreeMemory(device_, m.ib_mem, nullptr);
    m = {};
}

uint32_t StageRenderer::find_memory_type_(uint32_t filter,
                                          VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(vk_->physical_device(), &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool StageRenderer::create_buffer_(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags props,
                                   VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bi, nullptr, &buf) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, buf, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = find_memory_type_(req.memoryTypeBits, props);
    if (ai.memoryTypeIndex == UINT32_MAX) return false;
    if (vkAllocateMemory(device_, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindBufferMemory(device_, buf, mem, 0);
    return true;
}

VkShaderModule StageRenderer::load_shader_(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open()) {
        std::fprintf(stderr, "[StageRenderer] Cannot open shader: %s\n",
                     path.c_str());
        return VK_NULL_HANDLE;
    }
    size_t sz = static_cast<size_t>(f.tellg());
    std::vector<char> code(sz);
    f.seekg(0);
    f.read(code.data(), static_cast<std::streamsize>(sz));
    VkShaderModuleCreateInfo mi{};
    mi.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mi.codeSize = sz;
    mi.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &mi, nullptr, &mod) != VK_SUCCESS) {
        std::fprintf(stderr, "[StageRenderer] vkCreateShaderModule failed: %s\n",
                     path.c_str());
    }
    return mod;
}

bool StageRenderer::create_descriptor_set_() {
    VkDescriptorSetLayoutBinding b{};
    b.binding         = 0;
    b.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings    = &b;
    if (vkCreateDescriptorSetLayout(device_, &li, nullptr, &desc_layout_)
            != VK_SUCCESS) {
        return false;
    }

    const VkMemoryPropertyFlags hv =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!create_buffer_(sizeof(SceneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hv,
                        ubo_buf_, ubo_mem_)) {
        return false;
    }

    VkDescriptorPoolSize ps{};
    ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pi{};
    pi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets       = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes    = &ps;
    if (vkCreateDescriptorPool(device_, &pi, nullptr, &desc_pool_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = desc_pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &desc_layout_;
    if (vkAllocateDescriptorSets(device_, &ai, &desc_set_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorBufferInfo bi{};
    bi.buffer = ubo_buf_;
    bi.offset = 0;
    bi.range  = sizeof(SceneUBO);
    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = desc_set_;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo     = &bi;
    vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
    return true;
}

bool StageRenderer::create_pipeline_(const std::string& shader_dir) {
    VkShaderModule vert = load_shader_(shader_dir + "/stage.vert.spv");
    VkShaderModule frag = load_shader_(shader_dir + "/stage.frag.spv");
    if (!vert || !frag) {
        if (vert) vkDestroyShaderModule(device_, vert, nullptr);
        if (frag) vkDestroyShaderModule(device_, frag, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName  = "main";

    VkVertexInputBindingDescription bind{};
    bind.binding   = 0;
    bind.stride    = sizeof(StageVertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0].location = 0;
    attrs[0].binding  = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(StageVertex, pos);
    attrs[1].location = 1;
    attrs[1].binding  = 0;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(StageVertex, normal);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &bind;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions    = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_BACK_BIT;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // render_pass_ が外部から注入されているとき (post-process の HDR scene
    // pass 等) は depth attachment を持つので深度テスト/書き込みを有効化する。
    // 未指定で default render pass にフォールバックする場合は depth 無しなので
    // 無効のまま。
    if (render_pass_ != VK_NULL_HANDLE) {
        ds.depthTestEnable  = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
    } else {
        ds.depthTestEnable  = VK_FALSE;
        ds.depthWriteEnable = VK_FALSE;
    }

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &ba;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{};
    dy.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount = 2;
    dy.pDynamicStates    = dyn;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset     = 0;
    pcr.size       = sizeof(PushConsts);

    VkPipelineLayoutCreateInfo pli{};
    pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount         = 1;
    pli.pSetLayouts            = &desc_layout_;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges    = &pcr;
    if (vkCreatePipelineLayout(device_, &pli, nullptr, &pipeline_layout_)
            != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert, nullptr);
        vkDestroyShaderModule(device_, frag, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo gpi{};
    gpi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpi.stageCount          = 2;
    gpi.pStages             = stages;
    gpi.pVertexInputState   = &vi;
    gpi.pInputAssemblyState = &ia;
    gpi.pViewportState      = &vs;
    gpi.pRasterizationState = &rs;
    gpi.pMultisampleState   = &ms;
    gpi.pDepthStencilState  = &ds;
    gpi.pColorBlendState    = &cb;
    gpi.pDynamicState       = &dy;
    gpi.layout              = pipeline_layout_;
    gpi.renderPass          = render_pass_ ? render_pass_
                                           : vk_->default_render_pass();
    gpi.subpass             = 0;

    VkResult r = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gpi,
                                           nullptr, &pipeline_);
    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);
    if (r != VK_SUCCESS) {
        std::fprintf(stderr,
                     "[StageRenderer] vkCreateGraphicsPipelines failed (%d)\n",
                     static_cast<int>(r));
        return false;
    }
    return true;
}

#else   // !PICTOR_HAS_VULKAN && !ERGO_RENDER_HAS_VULKAN

StageRenderer::~StageRenderer() {}
void StageRenderer::initialize(RenderContext&) {}
void StageRenderer::set_render_pass(VkRenderPass) {}
void StageRenderer::update(const FrameContext&) {}
void StageRenderer::record(VkCommandBuffer, VkExtent2D) {}
void StageRenderer::shutdown() {}
StageMesh* StageRenderer::upload_mesh(const std::vector<StageVertex>&,
                                      const std::vector<uint32_t>&) {
    return nullptr;
}

#endif

} // namespace ergo::render
