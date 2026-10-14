#include "ergo/package_model/attribution.h"

#include "ergo/package_model/file_api_reader.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace ergo::package_model {

namespace {

constexpr const char* kErgo = "ergo";
constexpr const char* kHost = "host";

struct Root {
    std::string path;
    std::string package;
    bool        tree = false;   ///< ergo / host の根 (ビルドツリーの中は含めない)
};

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool all_under(const std::vector<std::string>& paths, const std::string& dir) {
    if (paths.empty()) return false;
    return std::all_of(paths.begin(), paths.end(),
                       [&](const std::string& p) { return starts_with(p, dir + "/"); });
}

/// パッケージの根からの相対ソース (根の外のソースは落とす)。
std::vector<std::string> relative_sources(const Target& t, const std::string& source_root,
                                          const std::string& root) {
    std::vector<std::string> out;
    for (const auto& s : t.sources) {
        const std::string abs = normalize_path(s, source_root);
        if (!root.empty() && path_within(abs, root) && abs.size() > root.size()) {
            out.push_back(abs.substr(root.size() + 1));
        }
    }
    return out;
}

void add_package(PackageModel& model, const std::string& name, PackageKind kind) {
    if (model.find_package(name)) return;
    Package p;
    p.name = name;
    p.kind = kind;
    model.packages.push_back(std::move(p));
}

} // namespace

bool excluded_by_default(const std::string& name, const std::vector<std::string>& rel) {
    if (name == "ergo_gtest_main") return true;
    if (starts_with(name, "test_") || all_under(rel, "tests")) return true;
    if (name.find("_dummy") != std::string::npos) return true;
    if (starts_with(name, "bench_") || all_under(rel, "benchmarks")) return true;
    return false;
}

void attribute_targets(PackageModel& model) {
    add_package(model, kErgo, PackageKind::Ergo);
    if (!model.has_codemodel) {
        std::sort(model.packages.begin(), model.packages.end(),
                  [](const Package& a, const Package& b) { return a.name < b.name; });
        return;
    }

    std::vector<Root> roots;
    for (const auto& p : model.packages) {
        if (p.kind == PackageKind::Link && !p.source_dir.empty()) {
            roots.push_back({normalize_path(p.source_dir), p.name});
        }
    }
    if (!model.ergo_root.empty()) roots.push_back({model.ergo_root, kErgo, true});
    const bool has_host = !model.source_root.empty() &&
                          !(model.ergo_root == model.source_root);
    if (has_host) roots.push_back({model.source_root, kHost, true});
    // ビルドツリーがソースの下にあっても (build/_deps/…)、 取得した第三者を ergo / host に数えない。
    const bool build_inside = !model.build_root.empty() && model.build_root != model.source_root &&
                              path_within(model.build_root, model.source_root);

    std::set<std::string> known;
    for (const auto& p : model.packages) known.insert(p.name);

    std::set<std::string> third_party;
    bool host_used = false;
    for (auto& t : model.targets) {
        const bool in_build = build_inside && path_within(t.source_dir, model.build_root);
        const Root* best = nullptr;
        for (const auto& r : roots) {
            if (r.tree && in_build) continue;
            if (path_within(t.source_dir, r.path) && (!best || r.path.size() > best->path.size())) {
                best = &r;
            }
        }
        std::string root_path;
        if (best) {
            t.package = best->package;
            root_path = best->path;
        } else if (!t.project.empty() && known.count(lower(t.project))) {
            t.package = lower(t.project);
            root_path = t.source_dir;
        } else if (!t.project.empty() &&
                   (in_build || !path_within(t.source_dir, model.source_root))) {
            t.package = lower(t.project);
            root_path = t.source_dir;
            third_party.insert(t.package);
        } else {
            t.package = has_host ? kHost : kErgo;
            root_path = has_host ? model.source_root : model.ergo_root;
        }
        if (t.package == kHost) host_used = true;
        t.excluded = excluded_by_default(t.name, relative_sources(t, model.source_root, root_path));
    }

    if (host_used) add_package(model, kHost, PackageKind::Host);
    for (const auto& name : third_party) add_package(model, name, PackageKind::ThirdParty);
    std::sort(model.packages.begin(), model.packages.end(),
              [](const Package& a, const Package& b) { return a.name < b.name; });
}

} // namespace ergo::package_model
