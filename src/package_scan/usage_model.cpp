#include "ergo/package_scan/usage_model.h"

namespace ergo::package_scan {

const char* to_string(UsageKind kind) {
    switch (kind) {
    case UsageKind::Musa:       return "musa";
    case UsageKind::Related:    return "related";
    case UsageKind::SharedTs:   return "shared_ts";
    case UsageKind::Ergo:       return "ergo";
    case UsageKind::Tool:       return "tool";
    case UsageKind::Unobserved: return "unobserved";
    }
    return "unobserved";
}

const char* to_string(UsageEdgeKind kind) {
    switch (kind) {
    case UsageEdgeKind::Submodule:        return "submodule";
    case UsageEdgeKind::FileDep:          return "file";
    case UsageEdgeKind::WorkspaceDep:     return "workspace";
    case UsageEdgeKind::ScopedDep:        return "scoped";
    case UsageEdgeKind::PortOf:           return "port-of";
    case UsageEdgeKind::SharesVocabulary: return "shares-vocabulary";
    }
    return "?";
}

bool is_dashed(UsageEdgeKind kind) {
    return kind == UsageEdgeKind::PortOf || kind == UsageEdgeKind::SharesVocabulary;
}

bool UsageEdge::dashed() const {
    for (const auto k : kinds) {
        if (!is_dashed(k)) return false;
    }
    return true;
}

const UsageNode* UsageModel::find_node(const std::string& id) const {
    for (const auto& n : nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const UsageEdge* UsageModel::find_edge(const std::string& from, const std::string& to) const {
    for (const auto& e : edges) {
        if (e.from == from && e.to == to) return &e;
    }
    return nullptr;
}

} // namespace ergo::package_scan
