#pragma once

// ergo_graph_layout の内部部品 (公開しない)。 段ごとにファイルを分ける:
//   layering.cpp — 戻り辺の検出と最長経路の列
//   ordering.cpp — 通り道 (仮の枠) の挿入と重心法による列内の順
//   routing.cpp  — 縦横の折れ線と座標の丸め
// どれもノードを入力順の添字で扱い、 同順位は ID の順位 (rank) で決める。

#include "ergo/graph_layout/layered_layout.h"

#include <cstddef>
#include <vector>

namespace ergo::graph_layout::detail {

/// 落とさなかった辺。 `left` は小さい列 (使う側)、 `right` は大きい列。
/// 戻り辺は `reversed` を立て、 経路を作った後に向きを戻す。
struct DagEdge {
    std::size_t from = 0, to = 0;       ///< 入力の向き (ノード添字)
    std::size_t left = 0, right = 0;    ///< 配置の向き
    bool        reversed = false;
    bool        dashed   = false;
};

/// ノード添字ごとの ID 順位 (0 = ID が最小)。
std::vector<std::size_t> id_ranks(const std::vector<LayoutNode>& nodes);

/// ID 順の深さ優先で戻り辺を見つけ、 `reversed` と `left` / `right` を埋める。
void orient_edges(std::size_t node_count, const std::vector<std::size_t>& rank,
                  std::vector<DagEdge>& edges);

/// 最長経路の列。 入り辺の無いノードが列 0。
std::vector<std::size_t> longest_path_columns(std::size_t node_count,
                                              const std::vector<std::size_t>& rank,
                                              const std::vector<DagEdge>& edges);

/// 列に置く枠。 ノード 1 つ、 または辺が途中の列を通るための仮の枠。
struct Slot {
    bool        dummy  = false;
    std::size_t node   = 0;   ///< dummy でなければノード添字
    std::size_t edge   = 0;   ///< dummy なら辺添字
    std::size_t column = 0;
    std::size_t position = 0; ///< 列内の順
    float       y = 0.0f, height = 0.0f;
};

/// 辺ごとの枠の並び (左端ノード → 通り道 … → 右端ノード)。
struct Chains {
    std::vector<Slot>                     slots;
    std::vector<std::vector<std::size_t>> by_edge;     ///< 辺添字 → 枠添字の列
    std::vector<std::vector<std::size_t>> by_column;   ///< 列 → 枠添字 (列内の順)
};

/// 枠を作り、 重心法 2 往復で列内の順を決める。
Chains order_columns(const std::vector<std::size_t>& column,
                     const std::vector<std::size_t>& rank,
                     const std::vector<DagEdge>& edges,
                     std::size_t columns);

/// 列 → x、 枠 → y・高さが決まった後に辺の経路 (配置の向きで左 → 右) を作る。
/// 戻り辺は向きを戻して返す。 座標の丸めと単純化は `finish_route` で行う。
std::vector<std::vector<tela_graph::Point>> route_edges(
    const Chains& chains, const std::vector<DagEdge>& edges,
    const std::vector<float>& column_x, const LayoutOptions& options);

/// 座標を 1/4 px に丸め、 重なる点と一直線上の中間点を除く (長さ 0 の区間を作らない)。
void finish_route(std::vector<tela_graph::Point>& points);

} // namespace ergo::graph_layout::detail
