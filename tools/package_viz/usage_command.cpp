#include "usage_command.h"

#include "ergo/package_model/file_api_reader.h"
#include "ergo/package_model/loader.h"
#include "ergo/package_scan/scan.h"

#include <cstdio>

namespace ergo::package_viz::cli {

namespace {

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

/// resolved.json に記録されたマニフェスト (Ergo のソース相対なら File API の Ergo の根で解く)。
bool manifest_from_build(const Options& o, std::string& manifest, std::string* error) {
    package_model::LoadOptions load;
    load.build_dir               = o.build_dir;
    load.resolved_path           = o.resolved;
    load.layers_path             = o.layers;
    load.codemodel.configuration = o.config;
    package_model::PackageModel model;
    if (!package_model::load_package_model(load, model, nullptr, error)) return false;
    if (model.manifest.empty()) return fail(error, "resolved.json records no manifest; pass --manifest");
    if (model.ergo_root.empty()) {
        return fail(error, "cannot locate the Ergo source without the File API reply; pass --manifest");
    }
    manifest = package_model::normalize_path(model.manifest, model.ergo_root);
    return true;
}

} // namespace

bool scan_from_options(const Options& o, package_scan::UsageModel& usage, std::string* error) {
    package_scan::ScanOptions scan;
    scan.root          = o.root;
    scan.manifest_path = o.manifest;
    if (scan.manifest_path.empty() && !manifest_from_build(o, scan.manifest_path, error)) return false;
    return package_scan::scan_usage(scan, usage, error);
}

int print_scan(const package_scan::UsageModel& usage) {
    for (const auto& e : usage.edges) {
        std::string kinds;
        for (const auto k : e.kinds) kinds += (kinds.empty() ? "" : "+") + std::string(package_scan::to_string(k));
        std::printf("%s -> %s [%s]%s\n", e.from.c_str(), e.to.c_str(), kinds.c_str(),
                    e.dashed() ? " dashed" : "");
        for (const auto& ev : e.evidence) {
            std::printf("    %s\n", package_scan::describe(ev).c_str());
        }
    }
    for (const auto& n : usage.nodes) {
        if (n.kind == package_scan::UsageKind::Unobserved) {
            std::printf("unobserved %s: %s\n", n.id.c_str(), n.reason.c_str());
        }
    }
    for (const auto& f : usage.files_read) std::printf("read %s\n", f.c_str());
    for (const auto& note : usage.notes) std::fprintf(stderr, "ergo_package_viz: %s\n", note.c_str());
    std::printf("ergo_package_viz: %zu usage edge(s), %zu node(s)\n", usage.edges.size(),
                usage.nodes.size());
    return 0;
}

} // namespace ergo::package_viz::cli
