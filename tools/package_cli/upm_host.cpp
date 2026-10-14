#include "upm_host.h"

#include "file_io.h"

#include "ergo/package_manager/init_plan.h"

namespace ergo::package_cli {

const char* const kUpmManifestPath = "Packages/manifest.json";

bool load_upm_declaration(const std::string& dir, pm::UpmDeclaration& out) {
    jsonm::JsonValue manifest;
    if (!read_json(join(dir, kUpmManifestPath), manifest, nullptr)) return false;
    out.dependencies = pm::read_upm_dependencies(manifest);
    jsonm::JsonValue host_json;
    if (read_json(join(dir, pm::kUpmHostPath), host_json, nullptr)) {
        out.code = pm::read_upm_host_code(host_json);
    }
    return true;
}

} // namespace ergo::package_cli
