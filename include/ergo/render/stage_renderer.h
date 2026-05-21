#pragma once

/// ergo_render — 共通レイヤー `StageRenderer`。
///
/// KuzuSurvivors の `src/render/stage_renderer.{h,cpp}` を ergo_render へ移植し、
/// `IRenderLayer` を実装する形に整えたもの。 最小限の 1 パイプライン:
/// per-frame Scene UBO (view/proj/lightDir) + per-object push constants
/// (model + baseColor)。 「色付きキューブ群」を描くだけの軽量描画層で、
/// AC / KS 共通の仮表示・フォールバック描画に使える。
///
/// IRenderLayer ライフサイクルへの対応:
///   set_render_pass() → 描画先 render pass を指定 (depth 付きパスなら深度有効)
///   initialize()      → descriptor / pipeline 構築
///   update(frame)     → Scene UBO (view/proj) を更新
///   record(cmd,ext)   → drawables を draw
///   shutdown()        → GPU リソース解放
///
/// drawable はゲーム側が `set_drawables()` で毎フレーム差し込む。 actor →
/// StageDrawable 変換はゲーム側の責務 (ergo_render はオーケストレーションのみ)。

#include "ergo/render/render_layer.h"
#include "ergo/render/vk_fwd.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace pictor { class VulkanContext; }

namespace ergo::render {

struct RenderContext;

/// 位置 + 法線の頂点。
struct StageVertex {
    float pos[3];
    float normal[3];
};

#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
/// GPU 上のメッシュ (vertex/index バッファ)。
struct StageMesh {
    VkBuffer       vb     = VK_NULL_HANDLE;
    VkBuffer       ib     = VK_NULL_HANDLE;
    VkDeviceMemory vb_mem = VK_NULL_HANDLE;
    VkDeviceMemory ib_mem = VK_NULL_HANDLE;
    uint32_t       index_count = 0;
};
#else
struct StageMesh { uint32_t index_count = 0; };
#endif

/// 1 個の描画対象 — メッシュ + ベース色 + モデル行列。
struct StageDrawable {
    const StageMesh* mesh = nullptr;
    float            color[4]  = {1, 1, 1, 1};
    float            model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};

/// 色キューブ描画レイヤー (1 パイプライン / 既定 depth なし)。
class StageRenderer : public IRenderLayer {
public:
    StageRenderer() = default;
    ~StageRenderer() override;

    StageRenderer(const StageRenderer&)            = delete;
    StageRenderer& operator=(const StageRenderer&) = delete;

    // ----- IRenderLayer -----------------------------------------------------
    /// RenderContext から VulkanContext と shader_dir を取り出して
    /// descriptor / pipeline を構築する。
    void initialize(RenderContext& ctx) override;

    /// 描画先 render pass を指定する。 initialize() より前に呼ぶこと。
    /// depth attachment を持つパス (post-process の HDR scene pass 等) を
    /// 渡すと深度テスト/書き込みが有効になる。 VK_NULL_HANDLE のままなら
    /// VulkanContext の default render pass (depth 無し) を使う。
    void set_render_pass(VkRenderPass render_pass) override;

    /// FrameContext の view/proj を Scene UBO へ反映する。
    void update(const FrameContext& frame) override;

    /// `set_drawables()` で渡された drawables を draw する。
    void record(VkCommandBuffer cmd, VkExtent2D extent) override;

    /// GPU リソースを解放する。
    void shutdown() override;

    // ----- StageRenderer 固有 ----------------------------------------------
    /// ライト方向 (xyz = toward-light, w = intensity)。 既定は Unity の
    /// デフォルト Directional Light 相当。 update() で UBO に書き込まれる。
    void set_light_dir(const float light_dir[4]);

    /// このフレームで描く drawable 列を差し込む。 StageRenderer は中身を
    /// コピーするので、 呼び出し側のバッファは即座に再利用してよい。
    void set_drawables(const StageDrawable* drawables, uint32_t count);
    void set_drawables(const std::vector<StageDrawable>& drawables);

    /// 頂点 / インデックス配列から GPU メッシュを 1 つ作る。 メッシュは
    /// StageRenderer が所有し、 shutdown() でまとめて破棄する。
    /// 戻り値は所有メッシュへのポインタ (StageDrawable.mesh に入れる)。
    StageMesh* upload_mesh(const std::vector<StageVertex>& verts,
                           const std::vector<uint32_t>&    idxs);

    bool initialized() const { return initialized_; }

private:
#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)
    void     destroy_mesh_(StageMesh& m);
    uint32_t find_memory_type_(uint32_t filter, VkMemoryPropertyFlags props);
    bool     create_buffer_(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags props,
                            VkBuffer& buf, VkDeviceMemory& mem);
    VkShaderModule load_shader_(const std::string& path);
    bool create_descriptor_set_();
    bool create_pipeline_(const std::string& shader_dir);

    pictor::VulkanContext* vk_              = nullptr;
    VkDevice               device_          = VK_NULL_HANDLE;
    VkRenderPass           render_pass_     = VK_NULL_HANDLE;
    VkPipeline             pipeline_        = VK_NULL_HANDLE;
    VkPipelineLayout       pipeline_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout  desc_layout_     = VK_NULL_HANDLE;
    VkDescriptorPool       desc_pool_       = VK_NULL_HANDLE;
    VkDescriptorSet        desc_set_        = VK_NULL_HANDLE;
    VkBuffer               ubo_buf_         = VK_NULL_HANDLE;
    VkDeviceMemory         ubo_mem_         = VK_NULL_HANDLE;
    std::deque<StageMesh>  owned_meshes_;
#endif
    // Unity デフォルト Directional Light (rotation 50,-30,0 / intensity 1.0) 相当。
    float                  light_dir_[4]    = {0.3215f, 0.766f, -0.5568f, 1.0f};
    std::vector<StageDrawable> drawables_;
    bool                   initialized_     = false;
};

/// 単位キューブ (一辺 1.0、 原点中心) を生成する。 24 頂点 / 36 インデックス、
/// 面ごとにフラット法線を持つ。 StageRenderer のフォールバック描画用。
void generate_unit_cube(std::vector<StageVertex>& verts,
                        std::vector<uint32_t>&    idxs);

} // namespace ergo::render
