#include "ergo/package_viz/reduction.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ergo::package_viz {

namespace {

using Pair = std::pair<std::string, std::string>;

/// from から to へ、 辺 skip を使わずに実線でたどれるか。
bool reachable_without(const std::map<std::string, std::set<std::string>>& out,
                       const std::string& from, const std::string& to, const Pair& skip) {
    std::set<std::string> seen{from};
    std::vector<std::string> todo{from};
    while (!todo.empty()) {
        const std::string n = todo.back();
        todo.pop_back();
        const auto it = out.find(n);
        if (it == out.end()) continue;
        for (const auto& m : it->second) {
            if (n == skip.first && m == skip.second) continue;
            if (m == to) return true;
            if (seen.insert(m).second) todo.push_back(m);
        }
    }
    return false;
}

} // namespace

std::size_t transitive_reduction(graph_layout::LayoutInput& graph) {
    std::map<std::string, std::set<std::string>> out;
    std::set<Pair> solid;
    std::size_t removed = 0;
    for (const auto& e : graph.edges) {
        if (e.dashed || e.from == e.to) continue;
        if (!solid.insert({e.from, e.to}).second) ++removed;   // 重複
        out[e.from].insert(e.to);
    }
    std::set<Pair> drop;
    for (const auto& p : solid) {   // (from, to) の順
        if (reachable_without(out, p.first, p.second, p)) {
            out[p.first].erase(p.second);
            drop.insert(p);
            ++removed;
        }
    }
    std::set<Pair> kept_solid;
    std::vector<graph_layout::LayoutEdge> edges;
    edges.reserve(graph.edges.size());
    for (const auto& e : graph.edges) {
        if (!e.dashed) {
            const Pair p{e.from, e.to};
            if (drop.count(p) || !kept_solid.insert(p).second) continue;
        }
        edges.push_back(e);
    }
    graph.edges = std::move(edges);
    return removed;
}

} // namespace ergo::package_viz
