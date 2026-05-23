#pragma once

/// ergo_render — Vulkan 型の集約ヘッダ。
///
/// ergo_render は Pictor (= 生 Vulkan) の上に乗るオーケストレーション層なので
/// 公開ヘッダで `VkRenderPass` / `VkCommandBuffer` / `VkExtent2D` /
/// `VkClearValue` といった Vulkan 型を使う。
///
/// Vulkan SDK が利用可能なビルド (`PICTOR_HAS_VULKAN` または
/// `ERGO_RENDER_HAS_VULKAN` が定義されている) では本物の `<vulkan/vulkan.h>`
/// を引く。 そうでないビルド — ユニットテストや Vulkan を持たない CI — でも
/// ergo_render のヘッダが parse でき、 Vulkan 非依存部分
/// (カメラ math / アセットパス解決 / ScreenshotBridge / FrameComposer の
/// パス列管理) をビルド・テストできるよう、 最小の互換型を定義する。
///
/// 互換型はあくまで「signature を保つためのプレースホルダ」であり、 実描画
/// 経路 (frame_composer.cpp / stage_renderer.cpp の Vulkan ブロック) は
/// SDK 有りのビルドでのみ有効になる。

#include <cstdint>

#if defined(PICTOR_HAS_VULKAN) || defined(ERGO_RENDER_HAS_VULKAN)

#include <vulkan/vulkan.h>

#else  // ---- Vulkan SDK 無し: 最小互換型 -----------------------------------

struct VkExtent2D {
    uint32_t width  = 0;
    uint32_t height = 0;
};

// ハンドル型はディスパッチャブル/非ディスパッチャブルとも不透明ポインタ扱い。
using VkRenderPass    = struct VkRenderPass_T*;
using VkCommandBuffer = struct VkCommandBuffer_T*;
using VkFramebuffer   = struct VkFramebuffer_T*;

// 本物の VkClearValue は union だが、 Vulkan 無しビルドでは中身を触らない
// (FrameComposer のパス列にぶら下がるだけ) ので最小 struct で足りる。
struct VkClearColorValue { float float32[4]; };
struct VkClearValue { VkClearColorValue color; };

#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE nullptr
#endif

#endif // PICTOR_HAS_VULKAN
