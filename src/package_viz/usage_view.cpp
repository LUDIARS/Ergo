#include "ergo/package_viz/usage_view.h"

#include "ergo/package_scan/scan.h"
#include "ergo/package_viz/groups.h"

#include <map>
#include <vector>

namespace ergo::package_viz {

namespace {

using package_scan::UsageKind;
using package_scan::UsageModel;
using package_scan::UsageNode;

const char* group_for(UsageKind kind) {
    switch (kind) {
    case UsageKind::Musa:       return kGroupMusa;
    case UsageKind::Related:    return kGroupRelated;
    case UsageKind::SharedTs:   return kGroupSharedTs;
    case UsageKind::Ergo:       return kGroupErgo;
    case UsageKind::Tool:       return kGroupTool;
    case UsageKind::Unobserved: return kGroupUnobserved;
    }
    return kGroupUnobserved;
}

std::string kinds_text(const package_scan::UsageEdge& e) {
    std::string out;
    for (const auto k : e.kinds) out += (out.empty() ? "" : "+") + std::string(package_scan::to_string(k));
    return out;
}

/// 札: 名前 (コード) / 種別か未観測の理由 / package.json の name / 出る辺の根拠 (上限まで)。
std::string label_for(const UsageNode& n, const UsageModel& usage) {
    std::string label = n.name;
    if (!n.code.empty()) label += " (" + n.code + ")";
    if (n.kind == UsageKind::Unobserved) {
        label += " | 未観測: " + n.reason;
        if (!n.declared_group.empty()) label += " · 宣言 " + n.declared_group;
    }
    if (!n.package_name.empty() && n.package_name != n.name) label += " | " + n.package_name;

    std::size_t shown = 0, rest = 0;
    for (const auto& e : usage.edges) {
        if (e.from != n.id) continue;
        for (const auto& ev : e.evidence) {
            if (shown < kUsageEvidenceLines) {
                label += " | → " + e.to + " [" + kinds_text(e) + "] " + package_scan::describe(ev);
                ++shown;
            } else {
                ++rest;
            }
        }
    }
    if (rest > 0) label += " | …他 " + std::to_string(rest) + " 件";
    return label;
}

} // namespace

ViewGraph build_usage_view(const UsageModel& usage, const ViewOptions& /*options*/) {
    ViewGraph view;
    auto& g   = view.graph;
    g.project = "Ergo";
    g.title   = "使用関係図 (観測)";
    g.groups  = usage_groups();

    std::size_t unobserved = 0;
    for (const auto& n : usage.nodes) {
        g.nodes.push_back({n.id, group_for(n.kind), label_for(n, usage)});
        view.info[n.id] = {n.id, {}, {}};
        if (n.kind == UsageKind::Unobserved) ++unobserved;
    }
    for (const auto& e : usage.edges) g.edges.push_back({e.from, e.to, e.dashed()});
    if (unobserved > 0) view.notes.push_back("未観測 " + std::to_string(unobserved));
    return view;
}

bool make_usage_view(const UsageModel& usage, const ViewOptions& options, FitResult& result,
                     std::string* error) {
    ViewOptions o = options;
    o.view = View::Usage;
    // 畳み込みは targets 図だけなので、 パッケージの構成は空でよい。
    return fit_view(build_usage_view(usage, o), package_model::PackageModel{}, o, result, error);
}

} // namespace ergo::package_viz
