#pragma once

/// ergo_render — `RenderContext` が実描画の前提を満たすかの検査。
///
/// `render_backend.h` が「ビルド時にどのバックエンド契約で組まれたか」を
/// 答えるのに対し、 このヘッダは「いま渡された `RenderContext` で実際に
/// 描けるか」を答える。 判定は必ず `RenderBackendError` の型付き値で返し、
/// 描けない状況を暗黙の成功へ縮退させない。
///
/// 判定順 (先に見つかった失敗を返す):
///   1. 実描画経路を含まないビルド → `VulkanBackendUnavailable`
///   2. `ctx.vk` が null            → `VulkanContextMissing`
///   3. `ctx.surface` が null       → `SurfaceProviderMissing`
///   4. サーフェスがネイティブウィンドウ未保持 → `SurfaceNotReady`

#include "ergo/render/render_backend.h"

namespace ergo::render {

struct RenderContext;

/// `ctx` で実描画できるかを検査する。 `RenderBackendError::None` なら
/// 実描画の前提を満たす。 それ以外は明示的な失敗。
RenderBackendError check_render_requirements(const RenderContext& ctx);

/// サーフェス供給者が現在ネイティブウィンドウを保持しているか。
/// Android のバックグラウンド遷移のように、 供給者は生きているが
/// ウィンドウが無い状態を区別するために使う。 供給者が null なら false。
bool surface_has_native_window(const RenderContext& ctx);

} // namespace ergo::render
