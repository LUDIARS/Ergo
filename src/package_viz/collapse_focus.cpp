#include "ergo/package_viz/collapse_focus.h"

#include "ergo/package_viz/groups.h"
#include "labels.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>

namespace ergo::package_viz {

namespace {

using package_model::PackageKind;
using package_model::PackageModel;

/// ID の付け替え後に辺を作り直す (自己ループを落とし、 重なりは実線が勝つ)。
void rebuild_edges(graph_layout::LayoutInput& g, const std::map<std::string, std::string>& remap) {
    std::vector<std::pair<std::string, std::string>> order;
    std::map<std::pair<std::string, std::string>, bool> dashed;
    for (const auto& e : g.edges) {
        const auto f = remap.find(e.from);
        const auto t = remap.find(e.to);
        const std::string from = f == remap.end() ? e.from : f->second;
        const std::string to   = t == remap.end() ? e.to : t->second;
        if (from == to) continue;
        const auto key = std::make_pair(from, to);
        const auto [it, inserted] = dashed.emplace(key, e.dashed);
        if (inserted) {
            order.push_back(key);
        } else if (!e.dashed) {
            it->second = false;
        }
    }
    g.edges.clear();
    for (const auto& key : order) g.edges.push_back({key.first, key.second, dashed[key]});
}

} // namespace

std::set<std::string> expanded_packages(const PackageModel& model,
                                        const std::vector<std::string>& expand) {
    std::set<std::string> out(expand.begin(), expand.end());
    for (const auto& p : model.packages) {
        if (p.kind == PackageKind::Host || p.kind == PackageKind::Ergo) out.insert(p.name);
    }
    return out;
}

std::vector<std::string> collapse_packages(ViewGraph& view, const PackageModel& model,
                                           const std::set<std::string>& expanded) {
    auto& g = view.graph;
    std::map<std::string, std::size_t> count;
    for (const auto& n : g.nodes) {
        const auto& info = view.info[n.id];
        if (!info.target.empty() && !expanded.count(info.package)) ++count[info.package];
    }
    if (count.empty()) return {};

    std::map<std::string, std::string> remap;
    std::vector<graph_layout::LayoutNode> nodes;
    std::map<std::string, NodeInfo> info;
    std::set<std::string> placed;
    for (const auto& n : g.nodes) {
        const NodeInfo ni = view.info[n.id];
        if (ni.target.empty() || expanded.count(ni.package)) {
            nodes.push_back(n);
            info[n.id] = ni;
            continue;
        }
        const std::string id = "pkg:" + ni.package;
        remap[n.id] = id;
        if (!placed.insert(id).second) continue;
        const auto* p = model.find_package(ni.package);
        const bool third = p && p->kind == PackageKind::ThirdParty;
        std::string label = id;
        if (p) label = detail::collapsed_label(*p, count[ni.package]);
        nodes.push_back({id, third ? kGroupThirdParty : kGroupExternal, label});
        info[id] = {ni.package, third ? kGroupThirdParty : kGroupExternal, {}};
    }
    g.nodes = std::move(nodes);
    view.info = std::move(info);
    rebuild_edges(g, remap);

    std::vector<std::string> collapsed;
    for (const auto& [name, n] : count) {
        collapsed.push_back(name + "(" + std::to_string(n) + ")");
    }
    return collapsed;
}

bool apply_focus(ViewGraph& view, const std::vector<std::string>& focus, std::size_t* dropped) {
    auto& g = view.graph;
    std::set<std::string> hit;
    for (const auto& n : g.nodes) {
        const auto& info = view.info[n.id];
        for (const auto& f : focus) {
            if (f == n.id || f == info.package || f == info.layer || f == info.target) {
                hit.insert(n.id);
            }
        }
    }
    if (hit.empty()) return false;
    std::set<std::string> keep = hit;
    for (const auto& e : g.edges) {
        if (hit.count(e.from)) keep.insert(e.to);
        if (hit.count(e.to)) keep.insert(e.from);
    }
    const std::size_t before = g.nodes.size();
    g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(),
                                 [&](const graph_layout::LayoutNode& n) { return !keep.count(n.id); }),
                  g.nodes.end());
    g.edges.erase(std::remove_if(g.edges.begin(), g.edges.end(),
                                 [&](const graph_layout::LayoutEdge& e) {
                                     return !keep.count(e.from) || !keep.count(e.to);
                                 }),
                  g.edges.end());
    for (auto it = view.info.begin(); it != view.info.end();) {
        it = keep.count(it->first) ? std::next(it) : view.info.erase(it);
    }
    if (dropped) *dropped = before - g.nodes.size();
    return true;
}

} // namespace ergo::package_viz
