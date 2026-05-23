#pragma once

/// ergo_render — サブレンダラ統一インターフェース `IRenderLayer`。
///
/// 各ゲームの「描画プリミティブ」 (色キューブ・skinned メッシュ・パーティクル
/// 等) は IRenderLayer を実装する。 FrameComposer はレイヤー実体を知らず、
/// このインターフェース越しに初期化順・更新・記録・破棄順を回す。
///
/// ライフサイクル (FrameComposer が駆動):
///   1. `initialize(RenderContext&)`      — GPU リソース確保。 add_pass 登録順。
///   2. `set_render_pass(VkRenderPass)`   — 所属パスの render pass を通知。
///                                          pipeline 構築前に呼ばれる。
///   3. `on_first_frame(RenderContext&)`  — 初回フレームで 1 回だけ。 遅延 bake
///                                          等の重い初期化をフレーム提示後に。
///   4. `update(const FrameContext&)`     — 毎フレーム頭。 drawlist 構築など。
///   5. `record(VkCommandBuffer, VkExtent2D)` — render pass 内でコマンド記録。
///   6. `shutdown()`                      — GPU リソース解放。 登録の逆順。

#include "ergo/render/frame_context.h"
#include "ergo/render/vk_fwd.h"

namespace ergo::render {

struct RenderContext;

/// サブレンダラの統一インターフェース。 ゲーム固有の描画レイヤーはこれを実装し、
/// FrameComposer のパス列に登録する。
class IRenderLayer {
public:
    virtual ~IRenderLayer() = default;

    /// GPU リソース (パイプライン / バッファ / ディスクリプタ) を確保する。
    /// FrameComposer が add_pass 登録順に呼ぶ。
    virtual void initialize(RenderContext& ctx) = 0;

    /// このレイヤーが属するパスの VkRenderPass を通知する。 パイプライン構築は
    /// render pass に依存するため、 `initialize()` の中ではなくこの通知後に
    /// 行う実装が多い。 FrameComposer が add_pass のパス情報から呼ぶ。
    virtual void set_render_pass(VkRenderPass render_pass) = 0;

    /// 初回フレームで 1 回だけ呼ばれる。 .riv の初回 bake のような重い初期化を
    /// 起動時ではなく「最初のフレームを提示した後」に回し、 黒画面フリーズを
    /// 避けるためのフック。 既定では何もしない。
    virtual void on_first_frame(RenderContext& ctx) { (void)ctx; }

    /// 毎フレームの頭で呼ばれる。 ゲームワールドから drawlist を組む、 UBO 用の
    /// 値を計算するなど、 コマンドバッファ記録より前の準備を行う。 既定では
    /// 何もしない。
    virtual void update(const FrameContext& frame) { (void)frame; }

    /// render pass の内側で描画コマンドを記録する。 `cmd` は begin 済みの
    /// コマンドバッファ、 `extent` は描画範囲。
    virtual void record(VkCommandBuffer cmd, VkExtent2D extent) = 0;

    /// GPU リソースを解放する。 FrameComposer が登録の逆順に呼ぶ。
    virtual void shutdown() = 0;
};

} // namespace ergo::render
