#include "ergo/package_viz/package_view.h"

#include "ergo/package_model/declaration_check.h"
#include "ergo/package_viz/groups.h"
#include "labels.h"

#include <map>
#include <set>
#include <utility>

namespace ergo::package_viz {

namespace {

using package_model::Package;
using package_model::PackageKind;
using package_model::PackageModel;

std::string group_for(const Package& p) {
    switch (p.kind) {
    case PackageKind::Host:       return kGroupHost;
    case PackageKind::Ergo:       return kGroupErgo;
    case PackageKind::Link:       return p.group == kGroupViz ? kGroupViz : kGroupRender;
    case PackageKind::Tool:       return kGroupTool;
    case PackageKind::ThirdParty: return kGroupThirdParty;
    case PackageKind::Observe:    return p.group == kGroupMusa ? kGroupMusa : kGroupRelated;
    }
    return kGroupRelated;
}

/// (from, to) → 実線か。 実線が 1 本でもあれば実線。
using EdgeSet = std::map<std::pair<std::string, std::string>, bool>;

void add_edge(EdgeSet& edges, const std::string& from, const std::string& to, bool dashed,
              const PackageModel& model) {
    if (from == to || !model.find_package(from) || !model.find_package(to)) return;
    auto [it, inserted] = edges.emplace(std::make_pair(from, to), dashed);
    if (!inserted && !dashed) it->second = false;
}

} // namespace

ViewGraph build_packages_view(const PackageModel& model, const ViewOptions& options) {
    ViewGraph view;
    auto& g   = view.graph;
    g.project = "Ergo";
    g.title   = "パッケージ版一覧 · " + (model.mode.empty() ? std::string("?") : model.mode);
    if (!model.platform.empty()) g.title += " · " + model.platform;
    g.groups = package_groups();

    std::set<std::string> undeclared_from;
    for (const auto& u : package_model::find_undeclared_links(model, options.include_tests)) {
        undeclared_from.insert(u.from_package);
    }
    std::map<std::string, std::size_t> target_count;
    for (const auto& t : model.targets) {
        if (!t.excluded || options.include_tests) ++target_count[t.package];
    }

    for (const auto& p : model.packages) {
        g.nodes.push_back({p.name, group_for(p),
                           detail::package_label(p, undeclared_from.count(p.name) > 0,
                                                 target_count[p.name], model)});
        view.info[p.name] = {p.name, {}, {}};
    }

    EdgeSet edges;
    for (const auto& p : model.packages) {
        for (const auto& r : p.requires_) add_edge(edges, p.name, r, false, model);
    }
    for (const auto& t : model.targets) {
        if (t.excluded && !options.include_tests) continue;
        for (const auto& id : t.dependencies) {
            const auto* dep = model.find_target_by_id(id);
            if (!dep || (dep->excluded && !options.include_tests)) continue;
            add_edge(edges, t.package, dep->package, false, model);
        }
    }
    for (const auto& m : model.modules) {
        for (const auto& from : m.ported_from) add_edge(edges, "ergo", from, true, model);
    }
    for (const auto& o : model.observe) {
        for (const auto& r : o.relations) {
            const std::string from = model.find_package(r.from) ? r.from : std::string("ergo");
            add_edge(edges, from, o.name, true, model);
        }
    }
    for (const auto& [ends, dashed] : edges) g.edges.push_back({ends.first, ends.second, dashed});
    return view;
}

} // namespace ergo::package_viz
