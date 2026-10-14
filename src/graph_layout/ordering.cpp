#include "layout_internal.h"

#include <algorithm>
#include <tuple>

namespace ergo::graph_layout::detail {

namespace {

/// 同じ重心の枠を並べる鍵。 ノードは ID 順、 通り道はノードの後に
/// (左端の ID, 右端の ID, 辺の入力順) で並べる。
using TieKey = std::tuple<int, std::size_t, std::size_t, std::size_t>;

TieKey tie_key(const Slot& s, const std::vector<std::size_t>& rank,
               const std::vector<DagEdge>& edges) {
    if (!s.dummy) return TieKey{0, rank[s.node], std::size_t{0}, std::size_t{0}};
    const auto& e = edges[s.edge];
    return TieKey{1, rank[e.left], rank[e.right], s.edge};
}

/// 隣の列 (direction = -1 なら左、 +1 なら右) にある相手の位置の平均。
/// 相手が無ければ今の位置を保つ。
float barycenter(const Chains& c, std::size_t slot, int direction,
                 const std::vector<std::vector<std::size_t>>& neighbors_left,
                 const std::vector<std::vector<std::size_t>>& neighbors_right) {
    const auto& list = direction < 0 ? neighbors_left[slot] : neighbors_right[slot];
    if (list.empty()) return static_cast<float>(c.slots[slot].position);
    float sum = 0.0f;
    for (const std::size_t n : list) sum += static_cast<float>(c.slots[n].position);
    return sum / static_cast<float>(list.size());
}

void sort_column(Chains& c, std::size_t column, int direction,
                 const std::vector<std::vector<std::size_t>>& left,
                 const std::vector<std::vector<std::size_t>>& right,
                 const std::vector<std::size_t>& rank, const std::vector<DagEdge>& edges) {
    auto& list = c.by_column[column];
    std::vector<std::pair<float, std::size_t>> keyed;
    keyed.reserve(list.size());
    for (const std::size_t s : list) keyed.emplace_back(barycenter(c, s, direction, left, right), s);
    std::sort(keyed.begin(), keyed.end(), [&](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first < b.first;
        return tie_key(c.slots[a.second], rank, edges) < tie_key(c.slots[b.second], rank, edges);
    });
    for (std::size_t i = 0; i < keyed.size(); ++i) {
        list[i] = keyed[i].second;
        c.slots[list[i]].position = i;
    }
}

} // namespace

Chains order_columns(const std::vector<std::size_t>& column,
                     const std::vector<std::size_t>& rank,
                     const std::vector<DagEdge>& edges,
                     std::size_t columns) {
    Chains c;
    const std::size_t node_count = column.size();
    c.slots.reserve(node_count);
    for (std::size_t n = 0; n < node_count; ++n) {
        Slot s;
        s.node   = n;
        s.column = column[n];
        c.slots.push_back(s);
    }
    c.by_edge.resize(edges.size());
    for (std::size_t e = 0; e < edges.size(); ++e) {
        const auto& edge = edges[e];
        auto& chain = c.by_edge[e];
        chain.push_back(edge.left);
        for (std::size_t col = column[edge.left] + 1; col < column[edge.right]; ++col) {
            Slot s;
            s.dummy  = true;
            s.edge   = e;
            s.column = col;
            chain.push_back(c.slots.size());
            c.slots.push_back(s);
        }
        chain.push_back(edge.right);
    }

    // 隣の列の相手 (鎖の区間はいつも隣り合う列を結ぶ)。
    std::vector<std::vector<std::size_t>> left(c.slots.size()), right(c.slots.size());
    for (const auto& chain : c.by_edge) {
        for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
            right[chain[i]].push_back(chain[i + 1]);
            left[chain[i + 1]].push_back(chain[i]);
        }
    }

    // 初期順: ID 順 (通り道はノードの後)。
    c.by_column.assign(columns, {});
    for (std::size_t s = 0; s < c.slots.size(); ++s) c.by_column[c.slots[s].column].push_back(s);
    for (auto& list : c.by_column) {
        std::sort(list.begin(), list.end(), [&](std::size_t a, std::size_t b) {
            return tie_key(c.slots[a], rank, edges) < tie_key(c.slots[b], rank, edges);
        });
        for (std::size_t i = 0; i < list.size(); ++i) c.slots[list[i]].position = i;
    }

    // 重心法 2 往復: 右向き (左の列を見る) → 左向き (右の列を見る) を 2 回。
    for (int round = 0; round < 2; ++round) {
        for (std::size_t col = 1; col < columns; ++col) {
            sort_column(c, col, -1, left, right, rank, edges);
        }
        for (std::size_t col = columns; col-- > 1;) {
            sort_column(c, col - 1, +1, left, right, rank, edges);
        }
    }
    return c;
}

} // namespace ergo::graph_layout::detail
