#pragma once

/// ergo_render — パス構成 + フレームループのオーケストレータ `FrameComposer`。
///
/// FrameComposer は「パス列」を組み、 1 フレームを回す横断層。 ゲームの
/// WorldRenderer が God Class 化していた部分 — 初期化順 / パス構成 /
/// acquire→record→submit→present / 初回フレームフック / 破棄順 — を
/// このクラスに集約する。
///
/// 使い方 (ホスト/ゲーム側):
///   FrameComposer fc;
///   fc.add_pass(scene_pass, { &stage_layer, &particle_layer });   // パス 1
///   fc.add_pass(hud_pass,   { &hud_layer });                      // パス 2
///   fc.initialize(render_context);
///   while (running) {
///       FrameContext frame = ...;          // dt / extent / camera を詰める
///       fc.run_frame(frame);
///   }
///   fc.shutdown();
///
/// パス列の組み方で描画構成を表現する (設計意図):
///   * PrivateGame の「postprocess 有効/無効 2 経路」 — 有効なら HDR scene パス +
///     post パス + HUD パスの 3 列、 無効なら swapchain default 1 列、 と
///     add_pass の呼び方を変えるだけで切り替わる。
///   * AC は最小構成 — default render pass 1 列に 1 レイヤーを載せるだけ。
///
/// FrameComposer 自身はゲームに依存しない。 パス列をどう組むか
/// (= どの VkRenderPass にどのレイヤーを並べるか) はホストが決める。

#include "ergo/render/frame_context.h"
#include "ergo/render/render_backend.h"
#include "ergo/render/render_layer.h"
#include "ergo/render/vk_fwd.h"

#include <cstdint>
#include <vector>

namespace ergo::render {

struct RenderContext;

/// 1 つの render pass と、 そのパス内で順に record されるレイヤー列。
struct RenderPassDesc {
    /// このパスの VkRenderPass。 swapchain default でも post-process の
    /// HDR scene pass でも、 ホストが渡したものをそのまま使う。
    VkRenderPass render_pass = VK_NULL_HANDLE;

    /// このパス内で `record()` される IRenderLayer 列 (記録順)。
    /// FrameComposer はレイヤーを所有しない (借用)。
    std::vector<IRenderLayer*> layers;

    /// vkCmdBeginRenderPass に渡す clear 値。 アタッチメント数に合わせる
    /// (color のみなら 1、 color+depth なら 2)。 空なら clearValueCount=0
    /// (LOAD で重ねる HUD パス等)。
    std::vector<VkClearValue> clear_values;
};

/// パス構成 + フレームループのオーケストレータ。
class FrameComposer {
public:
    FrameComposer() = default;
    ~FrameComposer();

    FrameComposer(const FrameComposer&)            = delete;
    FrameComposer& operator=(const FrameComposer&) = delete;

    /// パス列にパスを 1 つ追加する。 `initialize()` の前に呼ぶこと。
    /// add_pass の呼び出し順がパスの実行順になる。
    ///
    /// `clear_values` を省略すると color 1 枚を黒クリアする既定値を使う。
    /// HDR scene パス (color+depth) や LOAD パス (clear 無し) ではホストが
    /// 明示的に渡す。
    void add_pass(VkRenderPass render_pass,
                  std::vector<IRenderLayer*> layers);
    void add_pass(VkRenderPass render_pass,
                  std::vector<IRenderLayer*> layers,
                  std::vector<VkClearValue>  clear_values);

    /// 各レイヤーに framebuffer を解決させるための hook。 パスごとに
    /// 「swapchain image index → VkFramebuffer」を返す関数を登録する。
    /// 未登録のパスは VulkanContext の default framebuffers を使う。
    ///
    /// post-process の HDR scene パスのように image index に依存しない
    /// 固定 framebuffer も、 この hook が固定値を返せば表現できる。
    void set_framebuffer_provider(
        std::size_t pass_index,
        VkFramebuffer (*provider)(uint32_t image_index, void* user),
        void* user);

    /// パス `pass_index` を begin する **直前** に、 render pass の外側で
    /// コマンドを記録するための hook。 自前で vkCmdBeginRenderPass /
    /// EndRenderPass を行う合成処理 (post-process チェーン、 投影デカール
    /// 合成など) は IRenderLayer::record (= renderpass 内) には収まらないので、
    /// このパス間 hook で記録する。
    ///
    /// PrivateGame の post-process 経路: 「HDR scene パス」と「HUD パス」の 2 パスを
    /// add_pass で登録し、 HUD パス (index 1) の pre-pass hook に
    /// 「投影デカール合成 + post-process チェーン」を積む。 hook は HUD パスの
    /// vkCmdBeginRenderPass より前、 scene パスの vkCmdEndRenderPass より後に
    /// 呼ばれる。
    ///
    /// `pass_index == 0` の hook はフレーム先頭 (最初のパスの前) に呼ばれる。
    void set_pre_pass_hook(
        std::size_t pass_index,
        void (*hook)(VkCommandBuffer cmd, uint32_t image_index,
                     const FrameContext& frame, void* user),
        void* user);

    /// present 完了の **直後** に呼ばれる hook。 スクリーンショットの読み出し
    /// (提示済み swapchain image を読む) や、 初回フレーム提示後の遅延 bake の
    /// トリガなど、 「提示後に 1 回やる」処理を積む。 `image_index` は直近に
    /// present した swapchain image。
    void set_post_present_hook(
        void (*hook)(uint32_t image_index, const FrameContext& frame,
                     void* user),
        void* user);

    /// 全パスの全レイヤーを登録順に初期化する。
    ///   1. 各レイヤー initialize()
    ///   2. 各レイヤーへ所属パスの render pass を set_render_pass() で通知
    /// initialize 順は add_pass 登録順 × パス内レイヤー順 (従来どおり)。
    ///
    /// 戻り値は実描画の前提診断 (`check_render_requirements()` と同じ判定)。
    /// `RenderBackendError::None` 以外なら **このコンポーザは実際には
    /// 描かない**。 レイヤーの initialize 自体は Vulkan 非依存の初期化を
    /// 行えるよう従来どおり実行するが、 描けないことは戻り値と
    /// `last_error()` で明示する — 呼び出し側はこれを握り潰して
    /// 「描画できた」ことにしてはならない (KD-MOB-001)。
    ///
    /// 初期化済みの状態で再度呼んでも no-op で、 **初回 initialize 時の**
    /// 診断結果をそのまま返す (その後の run_frame が観測した一時的な失敗は
    /// 反映しない — そちらは `last_error()` を見ること)。
    RenderBackendError initialize(RenderContext& ctx);

    /// 直近の initialize() / run_frame() が記録した実描画の診断結果。
    /// 直近の検査が成功していれば `RenderBackendError::None`。
    /// run_frame() が Vulkan 呼び出しの失敗で継続不能を返した場合も、
    /// 対応する型付き値がここに残る。
    RenderBackendError last_error() const { return last_error_; }

    /// 1 フレームを回す。 内部シーケンス:
    ///   1. acquire_next_image()  — swapchain image を取得 (fence 待ちは内部)
    ///   2. 初回フレームなら全レイヤー on_first_frame()
    ///   3. 全レイヤー update(frame)
    ///   4. コマンドバッファ記録 — パスごとに begin/各レイヤー record/end
    ///   5. vkQueueSubmit + present
    /// 戻り値は継続可否 (false = フレームループを抜けるべき致命的失敗)。
    /// swapchain out-of-date や `SurfaceNotReady` 等の回復可能な状況では
    /// true を返してそのフレームをスキップする。
    /// initialize() 前の呼び出しや、実行中に surface が失われた場合の理由は
    /// `last_error()` で取得できる。
    bool run_frame(const FrameContext& frame);

    /// 全レイヤーを登録の **逆順** に shutdown() する。 二重呼び出しは no-op。
    void shutdown();

    /// 直近に present した swapchain image index。 未提示なら UINT32_MAX。
    uint32_t last_presented_index() const { return last_present_idx_; }

    /// これまでに run_frame() が回した累計フレーム数。
    uint64_t frame_count() const { return frame_count_; }

    /// 登録済みパス数。
    std::size_t pass_count() const { return passes_.size(); }

private:
    struct FramebufferProvider {
        VkFramebuffer (*fn)(uint32_t, void*) = nullptr;
        void* user = nullptr;
    };
    struct PrePassHook {
        void (*fn)(VkCommandBuffer, uint32_t, const FrameContext&, void*) = nullptr;
        void* user = nullptr;
    };
    struct PostPresentHook {
        void (*fn)(uint32_t, const FrameContext&, void*) = nullptr;
        void* user = nullptr;
    };

    std::vector<RenderPassDesc>      passes_;
    std::vector<FramebufferProvider> fb_providers_;
    std::vector<PrePassHook>         pre_pass_hooks_;
    PostPresentHook                  post_present_hook_;

    RenderContext* ctx_           = nullptr;
    bool           initialized_   = false;
    bool           first_frame_   = true;
    bool           shutdown_done_ = false;
    uint32_t       last_present_idx_ = UINT32_MAX;
    RenderBackendError last_error_    = RenderBackendError::None;
    /// initialize() 時点の前提診断。 run_frame() が last_error_ を毎フレーム
    /// 上書きするため、 initialize() の再呼び出しが返す値はこちらで保持する。
    RenderBackendError init_error_    = RenderBackendError::None;
    uint64_t       frame_count_      = 0;
};

} // namespace ergo::render
