#pragma once

/// ergo_render — per-frame mutable data + camera math.
///
/// `FrameContext` は 1 フレームごとに値が変わるデータをまとめた struct。
/// FrameComposer がフレーム頭で 1 つ組み立て、 各 IRenderLayer の
/// `update()` / `record()` 経路へ const 参照で渡す。 サブレンダラはここから
/// dt / カメラ行列 / 描画範囲 を読む。
///
/// カメラ math (look_at / Vulkan 用 perspective) もこのモジュールに同梱する。
/// ゲーム側のカメラ実装 (追従カメラ等) が eye / target / FOV を決め、
/// ここのフリー関数で view / proj 行列へ変換してから FrameContext に詰める。
///
/// `VkExtent2D` 等の Vulkan 型は `vk_fwd.h` 経由で取り込む (SDK 有りなら
/// 本物、 無しなら互換 struct)。

#include "ergo/render/vk_fwd.h"

#include <cstdint>

namespace ergo::render {

/// 毎フレーム可変のフレームデータ。 FrameComposer が組み立てて各レイヤーへ渡す。
struct FrameContext {
    /// 直前フレームからの経過秒数。
    float dt = 0.0f;

    /// 描画ターゲットの解像度 (通常は swapchain extent)。
    VkExtent2D extent{0, 0};

    /// カメラのビュー行列 (column-major float[16])。 ワールド→ビュー変換。
    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    /// カメラの射影行列 (column-major float[16])。 Vulkan NDC (Y down, depth [0,1])。
    float proj[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    /// アプリ起動からの累計フレーム番号。 初回フレームは 0。
    /// FrameComposer が `on_first_frame` を呼ぶかの判定にも使う。
    uint64_t frame_index = 0;
};

// ---------------------------------------------------------------------------
// カメラ math — column-major float[16] 行列を組むフリー関数群。
//
// すべて Vulkan 規約に揃える:
//   * 行列は column-major (OpenGL/GLSL の `mat4` と同じメモリレイアウト)。
//   * NDC は Vulkan の Y down / depth [0,1]。
// 行列の合成は `proj * view * model` の順 (GLSL のシェーダ内で行う想定)。
// ---------------------------------------------------------------------------

/// 3 要素ベクトル。 カメラ math 専用の最小型 (ergo_render は数学ライブラリを
/// 持ち込まない方針なので、 引数受け渡し用にこれだけ定義する)。
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

/// 度 → ラジアン変換。
float deg_to_rad(float deg);

/// 16 要素の単位行列を `out` に書く。
void mat4_identity(float out[16]);

/// 右手系 look-at ビュー行列を `out` (column-major float[16]) に書く。
///
/// `eye` がカメラ位置、 `center` が注視点、 `up` が上方向。 結果は
/// ワールド座標をビュー座標へ移す行列。
void look_at_rh(const Vec3& eye, const Vec3& center, const Vec3& up,
                float out[16]);

/// Vulkan 用の透視射影行列を `out` (column-major float[16]) に書く。
///
/// `fov_deg` は垂直方向の画角 (度)、 `aspect` は width/height、
/// `z_near` / `z_far` はクリップ面。 Vulkan NDC に合わせて Y を反転し、
/// 深度を [0,1] にマップする。
void perspective_vk(float fov_deg, float aspect, float z_near, float z_far,
                    float out[16]);

} // namespace ergo::render
