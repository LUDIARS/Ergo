#include "manifest_packages.h"

#include "json_fields.h"

namespace ergo::package_manager::detail {

std::vector<ManifestPackage> read_packages(const jsonm::JsonValue& doc,
                                           std::vector<Finding>& findings) {
    std::vector<ManifestPackage> out;
    const auto* pkgs = doc.find("packages");
    if (!pkgs || pkgs->is_null()) return out;
    if (!pkgs->is_object() || !pkgs->o) {
        manifest_error(findings, "packages", "packages must be an object");
        return out;
    }
    // JsonObject は std::map なので名前順に並ぶ。
    for (const auto& [name, entry] : *pkgs->o) {
        if (!entry.is_object()) {
            manifest_error(findings, name, "package entry must be an object");
            continue;
        }
        ManifestPackage p;
        p.name       = name;
        p.kind       = string_field(entry, "kind", name, findings);
        p.code       = string_field(entry, "code", name, findings);
        p.repository = string_field(entry, "repository", name, findings);
        p.rev        = string_field(entry, "rev", name, findings);
        p.expect_tag = string_field(entry, "expect_tag", name, findings);
        out.push_back(std::move(p));
    }
    return out;
}

} // namespace ergo::package_manager::detail
