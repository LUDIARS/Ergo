#include "layout_internal.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <utility>

namespace ergo::graph_layout::detail {

std::vector<std::size_t> id_ranks(const std::vector<LayoutNode>& nodes) {
    std::vector<std::size_t> order(nodes.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return nodes[a].id < nodes[b].id;
    });
    std::vector<std::size_t> rank(nodes.size());
    for (std::size_t r = 0; r < order.size(); ++r) rank[order[r]] = r;
    return rank;
}

void orient_edges(std::size_t node_count, const std::vector<std::size_t>& rank,
                  std::vector<DagEdge>& edges) {
    // 出辺をノードごとに、 行き先の ID 順 (同じ行き先は入力順) に並べる。
    std::vector<std::vector<std::size_t>> out(node_count);
    for (std::size_t e = 0; e < edges.size(); ++e) out[edges[e].from].push_back(e);
    for (auto& list : out) {
        std::stable_sort(list.begin(), list.end(), [&](std::size_t a, std::size_t b) {
            return rank[edges[a].to] < rank[edges[b].to];
        });
    }
    std::vector<std::size_t> roots(node_count);
    std::iota(roots.begin(), roots.end(), std::size_t{0});
    std::sort(roots.begin(), roots.end(),
              [&](std::size_t a, std::size_t b) { return rank[a] < rank[b]; });

    enum : unsigned char { kWhite, kGray, kBlack };
    std::vector<unsigned char> color(node_count, kWhite);
    std::vector<std::pair<std::size_t, std::size_t>> stack;   // (node, next out index)
    for (const std::size_t root : roots) {
        if (color[root] != kWhite) continue;
        color[root] = kGray;
        stack.emplace_back(root, 0);
        while (!stack.empty()) {
            auto& [node, next] = stack.back();
            if (next == out[node].size()) {
                color[node] = kBlack;
                stack.pop_back();
                continue;
            }
            const std::size_t e = out[node][next++];
            const std::size_t to = edges[e].to;
            if (color[to] == kGray) {
                edges[e].reversed = true;            // 戻り辺 (循環を切る)
            } else if (color[to] == kWhite) {
                color[to] = kGray;
                stack.emplace_back(to, 0);
            }
        }
    }
    for (auto& e : edges) {
        e.left  = e.reversed ? e.to : e.from;
        e.right = e.reversed ? e.from : e.to;
    }
}

std::vector<std::size_t> longest_path_columns(std::size_t node_count,
                                              const std::vector<std::size_t>& rank,
                                              const std::vector<DagEdge>& edges) {
    std::vector<std::vector<std::size_t>> out(node_count);
    std::vector<std::size_t> indegree(node_count, 0);
    for (const auto& e : edges) {
        out[e.left].push_back(e.right);
        ++indegree[e.right];
    }
    // Kahn 法。 取り出し順は ID 順 (列の値は順に依らないが、 走査を決定的に保つ)。
    auto by_rank = [&](std::size_t a, std::size_t b) { return rank[a] < rank[b]; };
    std::set<std::size_t, decltype(by_rank)> ready(by_rank);
    for (std::size_t n = 0; n < node_count; ++n) {
        if (indegree[n] == 0) ready.insert(n);
    }
    std::vector<std::size_t> column(node_count, 0);
    while (!ready.empty()) {
        const std::size_t n = *ready.begin();
        ready.erase(ready.begin());
        for (const std::size_t m : out[n]) {
            column[m] = std::max(column[m], column[n] + 1);
            if (--indegree[m] == 0) ready.insert(m);
        }
    }
    return column;
}

} // namespace ergo::graph_layout::detail
