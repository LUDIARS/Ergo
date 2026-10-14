#pragma once

/// ergo_package_viz — 分割 (上限に収める手順の 4 段目)。
///
/// ノードを (グループの列順, ID) で並べ、 1 枚のノード数が `node_limit` を、 その枚の中の
/// 辺が上限を超える手前で次の枚へ移る。 枚をまたぐ辺はどの枚にも描かず、 索引図に
/// 枚ごとのノード数・辺数・またぐ辺の本数を札で載せ、 枚同士を辺で結ぶ。

#include "ergo/graph_layout/layered_layout.h"
#include "ergo/package_viz/view_graph.h"

#include <cstddef>
#include <vector>

namespace ergo::package_viz {

struct SplitResult {
    std::vector<graph_layout::LayoutInput> parts;
    graph_layout::LayoutInput              index;
    std::size_t                            cut_edges = 0;   ///< 枚をまたいで描かなかった辺
};

/// `node_limit` は 1 以上 (`limits.nodes` より大きければ `limits.nodes`)。
SplitResult split_graph(const graph_layout::LayoutInput& graph, const Limits& limits,
                        std::size_t node_limit);

} // namespace ergo::package_viz
