#include "ergo/package_viz/host_view.h"

#include "ergo/package_viz/groups.h"
#include "host_labels.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace ergo::package_viz {

namespace {

using package_model::HostState;
using package_model::Package;
using package_model::PackageKind;
using package_model::WatchDependency;
using package_model::WatchUpdate;

constexpr const char* kHostId = "host";
constexpr const char* kErgoId = "ergo";

/// 由来の並び (畳んだノードの順)。 知らない値は unknown に寄せる。
const std::vector<std::string>& origin_order() {
    static const std::vector<std::string> order = {"pf", "ai", "human", "unknown"};
    return order;
}

std::string normalized_origin(const std::string& origin) {
    for (const auto& o : origin_order()) {
        if (o == origin) return o;
    }
    return "unknown";
}

/// 依存 1 つ: resolved.json と報告の両方から集める (名前順)。
struct DependencyRow {
    const Package*         package = nullptr;
    const WatchDependency* watched = nullptr;
};

std::map<std::string, DependencyRow> collect_dependencies(const HostState& s) {
    std::map<std::string, DependencyRow> rows;
    if (s.has_resolved) {
        for (const auto& p : s.resolved.packages) {
            if (p.kind != PackageKind::Link || p.state.empty() || p.state == "absent") continue;
            rows[p.name].package = &p;
        }
    }
    if (s.has_report) {
        for (const auto& d : s.report.dependencies) {
            if (d.name != kErgoId) rows[d.name].watched = &d;
        }
    }
    return rows;
}

std::string node_of(const std::string& dependency) {
    return dependency == kErgoId ? std::string(kErgoId) : "dep:" + dependency;
}

class HostViewBuilder {
public:
    explicit HostViewBuilder(const HostState& s) : s_(s) {}

    ViewGraph build() {
        auto& g   = view_.graph;
        g.project = "Ergo";
        g.title   = "ホスト状態図 " + detail::host_code(s_);
        g.groups  = host_groups();

        const WatchDependency* ergo = s_.has_report ? s_.report.find(kErgoId) : nullptr;
        add_node(kHostId, kGroupHost, detail::host_label(s_), kHostId);
        add_node(kErgoId, kGroupErgo, detail::ergo_label(s_, ergo), kErgoId);
        add_edge(kHostId, kErgoId, false);

        const auto rows = collect_dependencies(s_);
        for (const auto& [name, row] : rows) {
            add_node(node_of(name), kGroupDependency,
                     detail::dependency_label(name, row.package, row.watched), name);
            add_edge(kHostId, node_of(name), false);
        }

        std::vector<const WatchDependency*> watched;
        if (ergo) watched.push_back(ergo);
        for (const auto& [name, row] : rows) {
            if (row.watched) watched.push_back(row.watched);
        }
        add_updates(watched);
        add_reviews(watched);
        add_checks(rows);
        add_input_notes();
        return std::move(view_);
    }

private:
    void add_node(const std::string& id, const char* group, std::string label, const std::string& package) {
        if (!ids_.insert(id).second) return;
        view_.graph.nodes.push_back({id, group, std::move(label)});
        view_.info[id] = {package, {}, {}};
    }

    void add_edge(const std::string& from, const std::string& to, bool dashed) {
        if (!edges_.insert({from, to}).second) return;
        view_.graph.edges.push_back({from, to, dashed});
    }

    void add_updates(const std::vector<const WatchDependency*>& watched) {
        for (const auto* d : watched) {
            const std::string label = detail::update_label(*d);
            if (label.empty()) continue;
            const std::string id = "update:" + d->name;
            add_node(id, kGroupUpdate, d->name + " | " + label, d->name);
            add_edge(node_of(d->name), id, true);
        }
    }

    void add_reviews(const std::vector<const WatchDependency*>& watched) {
        std::vector<const WatchUpdate*> items;
        for (const auto* d : watched) {
            for (const auto& u : d->updates) {
                if (u.kind == "review" || u.kind == "run") items.push_back(&u);
            }
        }
        if (items.size() <= kHostReviewNodes) {
            for (const auto* u : items) {
                const std::string id = "review:" + u->dependency + ":" + u->subject;
                add_node(id, kGroupReview, detail::review_label(*u), u->dependency);
                add_edge(node_of(u->dependency), id, true);
            }
            return;
        }
        std::map<std::string, std::vector<const WatchUpdate*>> by_origin;
        for (const auto* u : items) by_origin[normalized_origin(u->origin)].push_back(u);
        for (const auto& origin : origin_order()) {
            const auto it = by_origin.find(origin);
            if (it == by_origin.end()) continue;
            const std::string id = "review:origin:" + origin;
            add_node(id, kGroupReview, detail::folded_review_label(origin, it->second), origin);
            for (const auto* u : it->second) add_edge(node_of(u->dependency), id, true);
        }
        view_.notes.push_back("審査中・作業中 " + std::to_string(items.size()) + " 件を由来ごとに畳み込み");
    }

    void add_checks(const std::map<std::string, DependencyRow>& rows) {
        if (!s_.has_resolved) return;
        for (const auto& f : s_.resolved.findings) {
            if (f.severity != "error" && f.severity != "warning") continue;
            const std::string id = "check:" + f.id + ":" + f.subject;
            add_node(id, kGroupCheck, detail::check_label(f), f.subject);
            const bool on_dependency = rows.count(f.subject) > 0 || f.subject == kErgoId;
            add_edge(on_dependency ? node_of(f.subject) : std::string(kHostId), id, true);
        }
    }

    void add_input_notes() {
        std::vector<std::string> missing;
        if (!s_.has_resolved) missing.push_back("resolved");
        if (!s_.has_report) missing.push_back("報告");
        if (!s_.has_status) missing.push_back("状態");
        if (!missing.empty()) {
            std::string text;
            for (const auto& m : missing) text += (text.empty() ? "" : "・") + m;
            view_.notes.push_back("未取得: " + text);
        }
        if (s_.has_report && s_.report.revisor == "unavailable") view_.notes.push_back("Revisor に届かず");
        if (s_.has_report && s_.report.concordia == "unavailable") view_.notes.push_back("Concordia に届かず");
    }

    const HostState&                            s_;
    ViewGraph                                   view_;
    std::set<std::string>                       ids_;
    std::set<std::pair<std::string, std::string>> edges_;
};

} // namespace

ViewGraph build_host_view(const HostState& state, const ViewOptions& /*options*/) {
    return HostViewBuilder(state).build();
}

bool make_host_view(const HostState& state, const ViewOptions& options, FitResult& result,
                    std::string* error) {
    ViewOptions o = options;
    o.view = View::Host;
    return fit_view(build_host_view(state, o), state.resolved, o, result, error);
}

} // namespace ergo::package_viz
