#include "ergo/package_manager/ergo_manifest.h"

#include "ergo/package_manager/host_declaration.h"
#include "manifest_packages.h"

#include <algorithm>

namespace ergo::package_manager {

namespace {

std::vector<std::string> object_keys(const jsonm::JsonValue& doc, const std::string& key) {
    std::vector<std::string> out;
    const auto* v = doc.find(key);
    if (v && v->is_object() && v->o) {
        for (const auto& kv : *v->o) out.push_back(kv.first);
    }
    return out;
}

} // namespace

const ManifestPackage* ErgoManifest::find(const std::string& name) const {
    for (const auto& p : packages) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

bool ErgoManifest::has_name(const std::string& name) const {
    return find(name) != nullptr ||
           std::find(modules.begin(), modules.end(), name) != modules.end() ||
           std::find(observe.begin(), observe.end(), name) != observe.end();
}

bool read_ergo_manifest(const jsonm::JsonValue& doc, ErgoManifest& out, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (!doc.is_object()) return fail("the Ergo manifest is not a JSON object");
    if (is_host_declaration(doc)) {
        return fail("the file has a `host` section: it is a host declaration, not an Ergo manifest");
    }
    const auto* schema = doc.find("schema");
    out.schema = (schema && schema->is_number()) ? static_cast<int>(schema->n) : 0;
    if (out.schema != 1) return fail("the Ergo manifest schema is not 1");

    std::vector<Finding> ignored;  // 型の不備は構成時の MANIFEST が正本。 ここは名前と pin だけ使う。
    out.packages = detail::read_packages(doc, ignored);
    out.modules  = object_keys(doc, "modules");
    out.observe  = object_keys(doc, "observe");
    return true;
}

} // namespace ergo::package_manager
