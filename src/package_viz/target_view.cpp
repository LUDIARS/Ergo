#include "ergo/package_viz/target_view.h"

#include "ergo/package_model/declaration_check.h"
#include "ergo/package_viz/groups.h"
#include "labels.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace ergo::package_viz {

namespace {

using package_model::PackageKind;
using package_model::PackageModel;
using package_model::Target;

std::string group_for(const Target& t, const PackageModel& model) {
    const auto* p = model.find_package(t.package);
    const PackageKind kind = p ? p->kind : PackageKind::Host;
    switch (kind) {
    case PackageKind::Ergo:       return t.layer.empty() ? kLayerShared : t.layer;
    case PackageKind::Host:       return t.layer.empty() ? kLayerApplication : t.layer;
    case PackageKind::ThirdParty: return kGroupThirdParty;
    default:                      return kGroupExternal;
    }
}

/// 既知の層でなければ shared に寄せる (グループを 8 から増やさない)。
std::string known_group(const std::string& g) {
    for (const auto& gr : target_groups()) {
        if (gr.id == g) return g;
    }
    return kLayerShared;
}

} // namespace

std::vector<std::vector<std::size_t>> cyclic_components(
    const std::vector<std::vector<std::size_t>>& adj) {
    // 反復版 Tarjan 法 (深い依存の鎖でもスタックを溢れさせない)。
    const std::size_t n = adj.size();
    constexpr std::size_t kUnset = static_cast<std::size_t>(-1);
    std::vector<std::size_t> index(n, kUnset), low(n, 0);
    std::vector<bool> on_stack(n, false);
    std::vector<std::size_t> stack;
    std::vector<std::vector<std::size_t>> out;
    std::size_t counter = 0;
    std::vector<std::pair<std::size_t, std::size_t>> work;   // (node, next edge)
    for (std::size_t root = 0; root < n; ++root) {
        if (index[root] != kUnset) continue;
        work.emplace_back(root, 0);
        while (!work.empty()) {
            const std::size_t v = work.back().first;
            std::size_t& next   = work.back().second;
            if (next == 0 && index[v] == kUnset) {
                index[v] = low[v] = counter++;
                stack.push_back(v);
                on_stack[v] = true;
            }
            if (next < adj[v].size()) {
                const std::size_t w = adj[v][next++];
                if (index[w] == kUnset) {
                    work.emplace_back(w, 0);
                } else if (on_stack[w]) {
                    low[v] = std::min(low[v], index[w]);
                }
                continue;
            }
            if (low[v] == index[v]) {
                std::vector<std::size_t> comp;
                std::size_t w = kUnset;
                do {
                    w = stack.back();
                    stack.pop_back();
                    on_stack[w] = false;
                    comp.push_back(w);
                } while (w != v);
                if (comp.size() > 1) {
                    std::sort(comp.begin(), comp.end());
                    out.push_back(std::move(comp));
                }
            }
            work.pop_back();
            if (!work.empty()) {
                const std::size_t parent = work.back().first;
                low[parent] = std::min(low[parent], low[v]);
            }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

ViewGraph build_targets_view(const PackageModel& model, const ViewOptions& options) {
    ViewGraph view;
    auto& g   = view.graph;
    g.project = "Ergo";
    g.title   = "ターゲットのリンク図";
    if (!model.configuration.empty()) g.title += " · " + model.configuration;
    g.groups = target_groups();

    std::set<std::string> undeclared;
    for (const auto& u : package_model::find_undeclared_links(model, options.include_tests)) {
        undeclared.insert(u.from_target);
    }

    // 残すターゲットと ID (同名が別パッケージにあれば "@パッケージ" を付ける)。
    std::vector<const Target*> kept;
    std::map<std::string, std::size_t> by_target_id;
    std::set<std::string> used_ids;
    std::vector<std::string> node_ids;
    for (const auto& t : model.targets) {
        if (t.excluded && !options.include_tests) continue;
        std::string id = t.name;
        if (!used_ids.insert(id).second) {
            id = t.name + "@" + t.package;
            used_ids.insert(id);
        }
        by_target_id[t.id] = kept.size();
        kept.push_back(&t);
        node_ids.push_back(std::move(id));
    }

    std::vector<std::vector<std::size_t>> adj(kept.size());
    for (std::size_t i = 0; i < kept.size(); ++i) {
        for (const auto& dep : kept[i]->dependencies) {
            const auto it = by_target_id.find(dep);
            if (it != by_target_id.end() && it->second != i) adj[i].push_back(it->second);
        }
    }
    std::vector<bool> in_cycle(kept.size(), false);
    const auto cycles = cyclic_components(adj);
    for (const auto& comp : cycles) {
        for (const std::size_t i : comp) in_cycle[i] = true;
    }
    if (!cycles.empty()) g.title += " · 循環 " + std::to_string(cycles.size()) + " 組";

    for (std::size_t i = 0; i < kept.size(); ++i) {
        const Target& t = *kept[i];
        const std::string layer = known_group(group_for(t, model));
        const std::string group = in_cycle[i] ? std::string(kGroupCycle) : layer;
        g.nodes.push_back({node_ids[i], group, detail::target_label(t, undeclared.count(t.name) > 0)});
        view.info[node_ids[i]] = {t.package, layer, t.name};
    }
    for (std::size_t i = 0; i < kept.size(); ++i) {
        for (const std::size_t j : adj[i]) g.edges.push_back({node_ids[i], node_ids[j], false});
    }
    return view;
}

} // namespace ergo::package_viz
