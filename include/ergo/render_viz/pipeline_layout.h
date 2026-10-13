#pragma once

/// ergo_render_viz — スナップショットを関係図へ配置する。
///
/// 行の並び (上から):
///   1. CPU 段     — DataUpdate → … → CommandEncode を左から右へ。 時間付き。
///                   `cpu_stages` が空なら行ごと出さない。
///   2. アタッチメント — パスが書く / 読む名前付き画像。 初出順。
///   3. render pass — 実行順に左から右へ。
///   4. エフェクト  — ポストプロセス・スタック。 最初の PostProcess パスの下から右へ。
///
/// 辺:
///   * パス同士は実行順に左から右へ実線で繋ぐ (書き込み先の宣言が無くても流れが見える)。
///   * パス → アタッチメント (書き込み) は実線、 アタッチメント → パス (読み取り) は破線。
///   * CPU 段は実線で連鎖し、 最後の段から最初のパスへ左の通路を通って繋ぐ。
///   * PostProcess パスから最初のエフェクトへ、 以降エフェクト同士を実線で連鎖。
///
/// 宣言されていないアタッチメントをパスが参照していれば、 そのノードを
/// 「未宣言」 と表示して残す (設定ミスを図で見つけられるように)。
///
/// 配置は決定的 (同じ入力から同じ図)。 Tela は位置を引き直さない。

#include "ergo/render_viz/pipeline_snapshot.h"
#include "ergo/tela_graph/graph.h"

namespace ergo::render_viz {

/// 図の寸法。 既定値は Tela の論理ピクセルで読める大きさ。
struct LayoutOptions {
    float node_width  = 200.0f;
    float node_height = 72.0f;
    float column_gap  = 48.0f;
    float row_gap     = 88.0f;
    float margin      = 32.0f;
    /// CPU 段から最初のパスへ降りる左の通路の幅。
    float lane_width  = 48.0f;
    /// 図の見出しに使うプロジェクト名 (`graph` 行の第 1 フィールド)。
    std::string project = "Pictor";
};

/// グループ ID (Tela の表示切り替え単位)。
inline constexpr const char* kGroupCpu        = "cpu";
inline constexpr const char* kGroupGeometry   = "geometry";
inline constexpr const char* kGroupShading    = "shading";
inline constexpr const char* kGroupPost       = "post";
inline constexpr const char* kGroupCompute    = "compute";
inline constexpr const char* kGroupAttachment = "attachment";

/// パス種別が属するグループ ID。
const char* group_of(PassKind kind);

/// スナップショットを配置済みの関係図にする。 ノードの無いグループは出さない。
/// パスが 1 本も無くてもアタッチメントか CPU 段があれば図にする。
tela_graph::Graph layout_pipeline(const PipelineSnapshot& snapshot,
                         const LayoutOptions&    options = {});

} // namespace ergo::render_viz
