#include "host_context.h"

#include "file_io.h"

#include <cstdlib>

namespace ergo::package_cli {

namespace {

const char* const kTemplatePath = "cmake/packages/ergo-bootstrap.cmake.in";
const char* const kManifestPath = "ergo-packages.json";

bool is_ergo_checkout(const std::string& dir) {
    return !dir.empty() && read_file(join(dir, kTemplatePath)).has_value();
}

void load_ergo(const Args& args, HostContext& out) {
    out.ergo_dir = locate_ergo(args);
    if (out.ergo_dir.empty()) {
        out.notes.push_back("Ergo checkout not found (pass --ergo <dir>)");
        return;
    }
    out.bootstrap_template = read_file(join(out.ergo_dir, kTemplatePath));
    jsonm::JsonValue doc;
    std::string error;
    if (!read_json(join(out.ergo_dir, kManifestPath), doc, &error)) {
        out.notes.push_back("Ergo manifest: " + error);
        return;
    }
    if (!pm::read_ergo_manifest(doc, out.ergo_manifest, &error)) {
        out.notes.push_back("Ergo manifest: " + error);
        return;
    }
    out.has_ergo_manifest = true;
}

} // namespace

std::string locate_ergo(const Args& args) {
    if (!args.ergo.empty()) return is_ergo_checkout(args.ergo) ? args.ergo : std::string();
    if (const char* env = std::getenv("ERGO_SOURCE_DIR"); env && is_ergo_checkout(env)) return env;
#if defined(ERGO_PKG_CLI_DEFAULT_ERGO_DIR)
    if (is_ergo_checkout(ERGO_PKG_CLI_DEFAULT_ERGO_DIR)) return ERGO_PKG_CLI_DEFAULT_ERGO_DIR;
#endif
    return {};
}

bool load_host_context(const Args& args, HostContext& out, std::string* error) {
    out.host_dir = args.host;
    load_ergo(args, out);

    const std::string path = join(args.host, kManifestPath);
    out.declaration_text = read_file(path);
    if (!out.declaration_text) return true;
    jsonm::JsonValue doc;
    if (!jsonm::parse(*out.declaration_text, doc)) {
        if (error) *error = path + ": not valid JSON";
        return false;
    }
    if (!pm::is_host_declaration(doc)) {
        out.notes.push_back(path + " has no `host` section (not a host declaration)");
        return true;
    }
    out.has_declaration = pm::read_host_declaration(doc, out.declaration, out.read_findings);
    return true;
}

} // namespace ergo::package_cli
