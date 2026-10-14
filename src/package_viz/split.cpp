#include "ergo/package_viz/split.h"

#include "ergo/package_viz/groups.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace ergo::package_viz {

namespace {

std::string part_id(std::size_t i) { return "part" + std::to_string(i + 1); }

} // namespace

SplitResult split_graph(const graph_layout::LayoutInput& g, const Limits& limits,
                        std::size_t node_limit) {
    node_limit = std::max<std::size_t>(1, std::min(node_limit, limits.nodes));
    std::map<std::string, std::size_t> group_order;
    for (std::size_t i = 0; i < g.groups.size(); ++i) group_order.emplace(g.groups[i].id, i);

    std::vector<std::size_t> order(g.nodes.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const auto ga = group_order.count(g.nodes[a].group_id) ? group_order[g.nodes[a].group_id]
                                                               : g.groups.size();
        const auto gb = group_order.count(g.nodes[b].group_id) ? group_order[g.nodes[b].group_id]
                                                               : g.groups.size();
        return ga != gb ? ga < gb : g.nodes[a].id < g.nodes[b].id;
    });

    // ノード ID → 隣 (自己ループを除く両向き。 同じ相手への辺は本数ぶん数える)。
    std::map<std::string, std::vector<std::string>> touching;
    for (const auto& e : g.edges) {
        if (e.from == e.to) continue;
        touching[e.from].push_back(e.to);
        touching[e.to].push_back(e.from);
    }

    std::map<std::string, std::size_t> part_of;
    std::vector<std::size_t> part_nodes, part_edges;
    for (const std::size_t idx : order) {
        const std::string& id = g.nodes[idx].id;
        std::size_t inner = 0;   // 今の枚に入れたら増える辺
        if (!part_nodes.empty()) {
            const std::size_t cur = part_nodes.size() - 1;
            for (const auto& other : touching[id]) {
                const auto it = part_of.find(other);
                if (it != part_of.end() && it->second == cur) ++inner;
            }
        }
        if (part_nodes.empty() || part_nodes.back() + 1 > node_limit ||
            part_edges.back() + inner > limits.edges) {
            part_nodes.push_back(0);
            part_edges.push_back(0);
            inner = 0;
        }
        part_of[id] = part_nodes.size() - 1;
        ++part_nodes.back();
        part_edges.back() += inner;
    }

    SplitResult out;
    const std::size_t count = part_nodes.size();
    out.parts.resize(count);
    for (std::size_t p = 0; p < count; ++p) {
        auto& part   = out.parts[p];
        part.project = g.project;
        part.title   = g.title + " (" + std::to_string(p + 1) + "/" + std::to_string(count) + ")";
        part.groups  = g.groups;
    }
    for (const auto& n : g.nodes) out.parts[part_of[n.id]].nodes.push_back(n);

    std::map<std::pair<std::size_t, std::size_t>, std::size_t> crossing;
    std::vector<std::size_t> cut_out(count, 0), cut_in(count, 0);
    for (const auto& e : g.edges) {
        const auto f = part_of.find(e.from);
        const auto t = part_of.find(e.to);
        if (f == part_of.end() || t == part_of.end()) continue;
        if (f->second == t->second) {
            out.parts[f->second].edges.push_back(e);
        } else {
            ++crossing[{f->second, t->second}];
            ++cut_out[f->second];
            ++cut_in[t->second];
            ++out.cut_edges;
        }
    }

    auto& index   = out.index;
    index.project = g.project;
    index.title   = g.title + " · 索引 (" + std::to_string(count) + " 枚、 またぐ辺 " +
                  std::to_string(out.cut_edges) + " 本)";
    index.groups  = index_groups();
    for (std::size_t p = 0; p < count; ++p) {
        std::set<std::string> names;
        for (const auto& n : out.parts[p].nodes) names.insert(n.group_id);
        std::string groups;
        for (const auto& gr : g.groups) {
            if (names.count(gr.id)) groups += (groups.empty() ? "" : ", ") + gr.name;
        }
        index.nodes.push_back({part_id(p), "part",
                               std::to_string(p + 1) + "/" + std::to_string(count) + " | ノード " +
                                   std::to_string(out.parts[p].nodes.size()) + " · 辺 " +
                                   std::to_string(out.parts[p].edges.size()) + " | " + groups +
                                   " | 出る " + std::to_string(cut_out[p]) + " 本 · 入る " +
                                   std::to_string(cut_in[p]) + " 本"});
    }
    for (const auto& [ends, n] : crossing) {
        (void)n;
        index.edges.push_back({part_id(ends.first), part_id(ends.second), false});
    }
    return out;
}

} // namespace ergo::package_viz
