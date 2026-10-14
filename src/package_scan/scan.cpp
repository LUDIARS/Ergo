#include "ergo/package_scan/scan.h"

#include "ergo/package_scan/manifest_reader.h"
#include "resolver.h"
#include "scope.h"
#include "source.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace ergo::package_scan {

namespace {

namespace fs = std::filesystem;
using detail::Source;

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::string sha7(const std::string& sha) { return sha.substr(0, 7); }

Source make_source(std::string node_id, detail::Scope scope, std::string package_dir, bool read_git) {
    Source s{std::move(node_id), std::move(scope)};
    s.package_dir = std::move(package_dir);
    s.read_git    = read_git;
    return s;
}

/// ノードと辺を集める。 辺は (from, to) ごとに種類と根拠を重ねる。
class Builder {
public:
    UsageNode& node(const std::string& id, UsageKind kind, const std::string& name = {}) {
        auto [it, inserted] = nodes_.emplace(id, UsageNode{});
        if (inserted) {
            it->second.id   = id;
            it->second.kind = kind;
            it->second.name = name.empty() ? id : name;
        }
        return it->second;
    }

    bool has_node(const std::string& id) const { return nodes_.count(id) > 0; }

    /// 依存先のノード (無ければ共有 TS として作る)。
    void ensure_target(const std::string& id) {
        if (has_node(id)) return;
        std::string name = id;
        if (name.rfind("npm:", 0) == 0) name = name.substr(4);
        if (name.rfind("repo:", 0) == 0) name = name.substr(5);
        node(id, UsageKind::SharedTs, name);
    }

    void edge(const std::string& from, const std::string& to, UsageEdgeKind kind, Evidence ev) {
        if (from.empty() || to.empty() || from == to) return;
        auto& e = edges_[{from, to}];
        e.from = from;
        e.to   = to;
        if (std::find(e.kinds.begin(), e.kinds.end(), kind) == e.kinds.end()) {
            e.kinds.push_back(kind);
            std::sort(e.kinds.begin(), e.kinds.end());
        }
        e.evidence.push_back(std::move(ev));
    }

    void finish(UsageModel& model) {
        for (auto& [id, n] : nodes_) model.nodes.push_back(std::move(n));
        for (auto& [ends, e] : edges_) model.edges.push_back(std::move(e));
    }

private:
    std::map<std::string, UsageNode>                            nodes_;
    std::map<std::pair<std::string, std::string>, UsageEdge>    edges_;
};

bool read_file(const std::string& path, std::string& out) {
    std::ifstream file(fs::u8path(path), std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

UsageKind observe_kind(const ManifestObserve& o) {
    return o.group == "musa" ? UsageKind::Musa : UsageKind::Related;
}

/// 観測対象を読めるか。 読めなければ理由。
std::string unobserved_reason(const ManifestObserve& o, const fs::path& root) {
    if (root.empty()) return "--root 未指定";
    if (!detail::is_safe_relative(o.path_hint)) return "path_hint が範囲外";
    std::error_code ec;
    if (!fs::is_directory(detail::normalized(root / fs::u8path(o.path_hint)), ec)) {
        return "チェックアウトなし";
    }
    return {};
}

void add_source_edges(const Source& s, const detail::Resolver& resolver, Builder& b) {
    for (const auto& sm : s.submodules) {
        const std::string to = resolver.for_submodule(sm);
        b.ensure_target(to);
        Evidence ev{s.gitmodules_file, sm.url_line ? sm.url_line : sm.path_line,
                    "submodule." + sm.name + (sm.url_line ? ".url" : ".path"), {}};
        if (s.gitlinks_known) {
            const auto it = s.gitlinks.find(sm.path);
            ev.note = it != s.gitlinks.end() ? "gitlink " + sha7(it->second) : "gitlink なし";
        } else {
            ev.note = "gitlink 未読";
        }
        b.edge(s.node_id, to, UsageEdgeKind::Submodule, std::move(ev));
    }
    if (!s.has_package) return;
    for (const auto& dep : s.package.deps) {
        const std::string to = resolver.for_dependency(s, dep);
        b.ensure_target(to);
        const UsageEdgeKind kind = dep.kind == DepSpecKind::File        ? UsageEdgeKind::FileDep
                                   : dep.kind == DepSpecKind::Workspace ? UsageEdgeKind::WorkspaceDep
                                                                        : UsageEdgeKind::ScopedDep;
        b.edge(s.node_id, to, kind,
               Evidence{s.package_file, dep.line, dep.section + "." + dep.name, dep.spec});
    }
}

/// モジュール名か観測対象名を辺の端にする (観測対象に無ければ Ergo のモジュール)。
std::string module_or_observe(const ScanManifest& m, const std::string& name, Builder& b) {
    for (const auto& o : m.observe) {
        if (o.name == name) return name;
    }
    b.node(name, UsageKind::Ergo);
    return name;
}

void add_manifest_edges(const ScanManifest& m, const std::string& manifest_file, Builder& b,
                        std::vector<std::string>& notes) {
    for (const auto& p : m.ports) {
        const std::string from = module_or_observe(m, p.module, b);
        b.ensure_target(p.package);
        b.edge(from, p.package, UsageEdgeKind::PortOf,
               Evidence{manifest_file, p.line, p.key, "ported_from " + p.package});
    }
    for (const auto& o : m.observe) {
        for (const auto& r : o.relations) {
            UsageEdgeKind kind;
            if (r.kind == "shares-vocabulary") {
                kind = UsageEdgeKind::SharesVocabulary;
            } else if (r.kind == "port-of" || r.kind == "ported-from") {
                kind = UsageEdgeKind::PortOf;
            } else {
                notes.push_back(manifest_file + ": " + r.key + ": unknown relation kind '" + r.kind +
                                "' — no edge");
                continue;
            }
            if (r.from.empty()) continue;
            const std::string from = module_or_observe(m, r.from, b);
            b.edge(from, o.name, kind, Evidence{manifest_file, r.line, r.key, r.kind});
        }
    }
}

} // namespace

std::vector<std::string> default_ergo_paths() { return {"tools/ergo"}; }

std::string describe(const Evidence& e) {
    std::string out = e.file;
    if (e.line > 0) out += ":" + std::to_string(e.line);
    if (!e.key.empty()) out += " " + e.key;
    if (!e.note.empty()) out += " (" + e.note + ")";
    return out;
}

bool scan_usage(const ScanOptions& o, UsageModel& model, std::string* error) {
    model = UsageModel{};
    std::string text;
    if (o.manifest_path.empty() || !read_file(o.manifest_path, text)) {
        return fail(error, "cannot read manifest " + o.manifest_path);
    }
    ScanManifest manifest;
    if (!parse_scan_manifest(text, manifest, error)) return false;
    const fs::path manifest_abs = detail::normalized(fs::u8path(o.manifest_path));
    const std::string manifest_file = "ergo:" + detail::generic_utf8(manifest_abs.filename());
    model.files_read.push_back(manifest_file);

    detail::Resolver resolver;
    resolver.manifest  = &manifest;
    resolver.ergo_root = o.ergo_root.empty() ? manifest_abs.parent_path()
                                             : detail::normalized(fs::u8path(o.ergo_root));
    if (!o.root.empty()) resolver.root = detail::normalized(fs::u8path(o.root));

    Builder b;
    b.node("ergo", UsageKind::Ergo, "Ergo").code = "Eg";
    for (const auto& t : manifest.tools) b.node(t.name, UsageKind::SharedTs).code = t.code;

    // 読み取り元: Ergo (submodule)、 Ergo の Web ツール、 観測対象。
    std::vector<Source> sources;
    sources.push_back(make_source("ergo", detail::Scope(resolver.ergo_root, "ergo:", &model.files_read),
                                  {}, true));
    for (const auto& ep : o.ergo_paths) {
        if (!detail::is_safe_relative(ep)) {
            model.notes.push_back("ergo path '" + ep + "' is outside Ergo — skipped");
            continue;
        }
        resolver.ergo_paths.push_back(ep);
        sources.push_back(make_source(ep, detail::Scope(resolver.ergo_root, "ergo:", &model.files_read),
                                      ep, false));
    }
    for (const auto& obs : manifest.observe) {
        auto& n = b.node(obs.name, observe_kind(obs));
        n.code           = obs.code;
        n.declared_group = obs.group;
        const std::string reason = unobserved_reason(obs, resolver.root);
        if (!reason.empty()) {
            n.kind   = UsageKind::Unobserved;
            n.reason = reason;
            continue;
        }
        sources.push_back(make_source(obs.name,
                                      detail::Scope(resolver.root / fs::u8path(obs.path_hint),
                                                    obs.path_hint, &model.files_read),
                                      {}, true));
    }

    for (auto& s : sources) detail::read_source(s, model.notes);
    for (const auto& s : sources) {
        if (s.node_id != "ergo" && s.has_package && !s.package.name.empty()) {
            resolver.npm_names.emplace(s.package.name, s.node_id);
        }
        // 観測対象のノードは既にある (種類はそのまま)。 Ergo の Web ツールはここで作る。
        if (s.has_package && s.node_id != "ergo") {
            b.node(s.node_id, UsageKind::Tool).package_name = s.package.name;
        }
    }
    for (const auto& s : sources) add_source_edges(s, resolver, b);
    add_manifest_edges(manifest, manifest_file, b, model.notes);

    b.finish(model);
    std::sort(model.files_read.begin(), model.files_read.end());
    model.files_read.erase(std::unique(model.files_read.begin(), model.files_read.end()),
                           model.files_read.end());
    return true;
}

} // namespace ergo::package_scan
