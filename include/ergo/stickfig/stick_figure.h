#pragma once

/// ergo_stickfig — 棒人間 (stick figure) の幾何学生成。
///
/// `ergo_render` の `StageRenderer` が「色付きプリミティブ群」を描く軽量描画層
/// であるのに対し、 本モジュールはその描画層に流し込める **CPU メッシュ** を
/// 純粋計算で組み立てる責務だけを持つ (Vulkan 非依存・他ドメイン非 import)。
///
/// 提供する生成器:
///   * `generate_sphere`       — UV 球 (頭部用)
///   * `generate_capsule`      — +Y 軸方向のカプセル (円柱 + 半球キャップ、 四肢用)
///   * `generate_stick_figure` — 頭 + 胴 + 両腕 + 両脚を組み上げた人型 1 体
///
/// 出力頂点 `Vertex` は `pos[3] + normal[3]` で、 `ergo::render::StageVertex` と
/// バイナリ互換にしてある。 描画側は `StageRenderer::upload_mesh()` に
/// コピーでそのまま渡せる (本モジュールは render に依存しないため、 変換は
/// あくまで consumer 側の責務)。

#include <cstdint>
#include <vector>

namespace ergo::stickfig {

/// 位置 + 法線の頂点。 `ergo::render::StageVertex` とレイアウト互換。
struct Vertex {
    float pos[3];
    float normal[3];
};

/// CPU メッシュ — インターリーブ頂点 + 三角形インデックス。
struct Mesh {
    std::vector<Vertex>   verts;
    std::vector<uint32_t> idxs;
};

/// 原点中心の UV 球を生成する。 `segments` = 経度方向の分割、 `rings` =
/// 緯度方向の分割。 頭部メッシュに使う。
Mesh generate_sphere(float radius, int segments = 16, int rings = 12);

/// +Y 軸方向・原点中心のカプセル (円柱 + 両端半球キャップ) を生成する。
/// 円柱部分の長さが `length`、 半径が `radius`。 全長は `length + 2*radius`。
/// `segments` = 円周方向の分割、 `cap_rings` = 各半球キャップの緯度分割。
/// 四肢・胴体のセグメントに使う。
Mesh generate_capsule(float radius, float length,
                      int segments = 12, int cap_rings = 4);

/// 棒人間 1 パーツ — メッシュ + 色 + 配置行列。
struct StickPart {
    const char* name;       ///< "head" / "torso" / "arm_l" など (デバッグ表示用)
    Mesh        mesh;       ///< 原点基準の生メッシュ
    float       color[4];   ///< RGBA
    float       model[16];  ///< 列優先 4x4 (StageDrawable.model と同じ並び)
};

/// 棒人間の体型パラメータ。 単位はメートル想定 (描画スケールは consumer 側)。
struct StickFigureParams {
    float height      = 1.8f;   ///< 直立全高 (足元 y=0 → 頭頂 y=height)
    float arm_span    = 1.6f;   ///< 指先〜指先の横幅
    float limb_radius = 0.055f; ///< 四肢・胴カプセルの半径
    float head_radius = 0.13f;  ///< 頭部球の半径
    int   segments    = 12;     ///< 円周方向のテッセレーション
};

/// 頭 (球) + 胴 + 両腕 + 両脚 (カプセル) からなる人型 1 体を、 各パーツの
/// メッシュ・色・配置行列付きで組み立てて返す。 描画側は各 `StickPart` を
/// `StageDrawable` (mesh = upload 済みメッシュ、 color/model = 本構造体の値) に
/// 変換して `StageRenderer` へ渡せばよい。
std::vector<StickPart> generate_stick_figure(const StickFigureParams& params = {});

} // namespace ergo::stickfig
