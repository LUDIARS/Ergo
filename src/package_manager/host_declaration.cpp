#include "ergo/package_manager/host_declaration.h"

#include "json_fields.h"
#include "manifest_packages.h"

namespace ergo::package_manager {

bool is_host_declaration(const jsonm::JsonValue& doc) {
    return doc.is_object() && doc.find("host") != nullptr;
}

bool read_host_declaration(const jsonm::JsonValue& doc, HostDeclaration& out,
                           std::vector<Finding>& findings) {
    if (!is_host_declaration(doc)) {
        detail::manifest_error(findings, "host", "not a host declaration (no `host` section)");
        return false;
    }
    const auto* schema = doc.find("schema");
    out.schema = (schema && schema->is_number()) ? static_cast<int>(schema->n) : 0;
    if (out.schema != 1) detail::manifest_error(findings, "schema", "schema must be 1");

    const auto& host = *doc.find("host");
    if (!host.is_object()) {
        detail::manifest_error(findings, "host", "host must be an object");
        return true;
    }
    out.code = detail::string_field(host, "code", "host", findings);

    if (const auto* ergo = host.find("ergo"); ergo && !ergo->is_null()) {
        if (!ergo->is_object()) {
            detail::manifest_error(findings, "ergo", "host.ergo must be an object");
        } else {
            out.ergo.present    = true;
            out.ergo.repository = detail::string_field(*ergo, "repository", "ergo", findings);
            out.ergo.rev        = detail::string_field(*ergo, "rev", "ergo", findings);
            out.ergo.expect_tag = detail::string_field(*ergo, "expect_tag", "ergo", findings);
            out.ergo.track_branch = detail::read_track_branch(*ergo, "ergo", findings);
        }
    }
    out.use         = detail::string_array_field(host, "use", "host", findings);
    out.optional    = detail::string_array_field(host, "optional", "host", findings);
    out.features    = detail::string_map_field(host, "features", "host", findings);
    out.legacy_vars = detail::string_map_field(host, "legacy_vars", "host", findings);
    out.packages    = detail::read_packages(doc, findings);
    return true;
}

} // namespace ergo::package_manager
