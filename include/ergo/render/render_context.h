#pragma once

/// ergo_render — Pictor ハンドル束。
///
/// `RenderContext` はサブレンダラ (IRenderLayer 実装) の初期化に必要な
/// Pictor のハンドル一式を 1 つにまとめた struct。 ホスト (ゲーム) が
/// Pictor を立ち上げたあとに作り、 FrameComposer / 各レイヤーの
/// `initialize()` に const でない参照で渡す。
///
/// 所有関係: RenderContext はどのハンドルも **所有しない** (借用のみ)。
/// VulkanContext / GlfwSurfaceProvider 等の生存期間はホストが管理する。
///
/// 設計判断 (承認済み): Pictor は「生 Vulkan の VulkanContext」と「高レベルの
/// PictorRenderer」の二重 API を持つが、 ergo_render は当面 VulkanContext を
/// 基盤とする。 `renderer` (PictorRenderer) と `anim` (AnimationSystem) は
/// 任意パススルーで、 使わないレイヤーは無視してよい (nullptr 許容)。

#include <string>

namespace pictor {
class VulkanContext;
class GlfwSurfaceProvider;
class PictorRenderer;
class AnimationSystem;
} // namespace pictor

namespace ergo::render {

/// サブレンダラ初期化時に渡す Pictor ハンドル束。
struct RenderContext {
    /// 生 Vulkan のコンテキスト (instance/device/swapchain/同期オブジェクト)。
    /// ergo_render のフレームループの基盤。 必須 (非 null を想定)。
    pictor::VulkanContext* vk = nullptr;

    /// ウィンドウ/サーフェス供給者。 イベントポーリングや should_close の
    /// 問い合わせに使う。 デスクトップでは GlfwSurfaceProvider 実体。
    pictor::GlfwSurfaceProvider* surface = nullptr;

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
