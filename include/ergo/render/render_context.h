#pragma once

/// ergo_render — Pictor ハンドル束。
///
/// `RenderContext` はサブレンダラ (IRenderLayer 実装) の初期化に必要な
/// Pictor のハンドル一式を 1 つにまとめた struct。 ホスト (ゲーム) が
/// Pictor を立ち上げたあとに作り、 FrameComposer / 各レイヤーの
/// `initialize()` に const でない参照で渡す。
///
/// 所有関係: RenderContext はどのハンドルも **所有しない** (借用のみ)。
/// VulkanContext / ISurfaceProvider 実装等の生存期間はホストが管理する。

/// プラットフォーム中立: `surface` は Pictor の抽象 `ISurfaceProvider` を
/// 借用する。 デスクトップの `GlfwSurfaceProvider`、 Android の
/// `AndroidSurfaceProvider`、 iOS の `IOSSurfaceProvider` はいずれもこの
/// インターフェースの実装なので、 ergo_render の公開ヘッダは具象型を
/// 一切要求しない (KD-MOB-001)。
///
/// 設計判断 (承認済み): Pictor は「生 Vulkan の VulkanContext」と「高レベルの
/// PictorRenderer」の二重 API を持つが、 ergo_render は当面 VulkanContext を
/// 基盤とする。 `renderer` (PictorRenderer) と `anim` (AnimationSystem) は
/// 任意パススルーで、 使わないレイヤーは無視してよい (nullptr 許容)。

#include <string>

namespace pictor {
class VulkanContext;
class ISurfaceProvider;
class PictorRenderer;
class AnimationSystem;
} // namespace pictor

namespace ergo::render {

/// サブレンダラ初期化時に渡す Pictor ハンドル束。
struct RenderContext {
    /// 生 Vulkan のコンテキスト (instance/device/swapchain/同期オブジェクト)。
    /// ergo_render のフレームループの基盤。 必須 (非 null を想定)。
    pictor::VulkanContext* vk = nullptr;

    /// ウィンドウ/サーフェス供給者 (プラットフォーム中立)。 ネイティブ
    /// ハンドル / swapchain 設定の問い合わせ、 イベントポーリングや
    /// should_close に使う。 実体はデスクトップなら `GlfwSurfaceProvider`、
    /// Android なら `AndroidSurfaceProvider`、 iOS なら `IOSSurfaceProvider`
    /// だが、 ergo_render はどれかを知らない。 実描画には非 null が必須で、
    /// 不足は `check_render_requirements()` が型付き失敗として返す。
    pictor::ISurfaceProvider* surface = nullptr;

    /// 高レベル Pictor レンダラ。 任意 — VulkanContext だけで描く
    /// レイヤーは使わない。 使うレイヤーだけが参照する。
    pictor::PictorRenderer* renderer = nullptr;

    /// スケルタルアニメーションシステム。 任意 — skinning を行う
    /// レイヤーだけが参照する。
    pictor::AnimationSystem* anim = nullptr;

    /// 解決済みのシェーダディレクトリ (末尾スラッシュ無し)。
    /// `*.spv` ファイルがここに置かれている前提。 通常は
    /// `resolve_shader_dir()` の戻り値を入れる。
    std::string shader_dir;

    /// 解決済みのアセットルート (末尾スラッシュ無し)。 モデル / テクスチャ /
    /// VFX 定義などのゲームアセットの基準ディレクトリ。 通常は
    /// `resolve_asset_root()` の戻り値を入れる。
    std::string asset_root;
};

} // namespace ergo::render
