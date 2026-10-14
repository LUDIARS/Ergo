// plan_remove — host.use / optional / features と非公開 packages から外す。 needs を壊すなら拒む。

#include "ergo/package_manager/declaration_edit.h"
#include "ergo/package_manager/package_owner.h"

#include "declaration_edit_support.h"

namespace ergo::package_manager {

namespace {

using detail::edit_error;

std::string join_names(const std::vector<std::string>& names) {
    std::string out;
    for (const auto& n : names) out += (out.empty() ? "" : ", ") + n;
    return out;
}

} // namespace

EditResult plan_remove(const std::string& host_text, const ErgoManifest* ergo, const std::string& name) {
    if (name == kErgoPackageName) return edit_error("Ergo itself cannot be removed from the host declaration");
    HostDeclaration host;
    std::string error;
    if (!detail::parse_host_text(host_text, host, error)) return edit_error(error);

    const bool is_private = owner_of(name, &host, nullptr) == PackageOwner::HostPrivate;
    const bool declared   = is_private || detail::contains(host.use, name) ||
                          detail::contains(host.optional, name) || host.features.count(name) != 0;
    if (!declared) {
        EditResult r;
        r.text = host_text;
        r.notes.push_back("'" + name + "' is not declared by the host: nothing to change");
        return r;
    }
    if (!ergo) return edit_error("the Ergo manifest is needed to check modules.*.needs (pass --ergo)");
    const auto needing = ergo->modules_needing(name);
    if (!needing.empty()) {
        return edit_error("'" + name + "' is needed by " + join_names(needing) +
                          " (modules.*.needs in the Ergo manifest); turn those modules off in the host "
                          "build instead of removing the package");
    }

    std::string text = host_text;
    bool ok = !json_has(text, {"host", "use"}) || json_remove_string(text, {"host", "use"}, name);
    ok = ok && (!json_has(text, {"host", "optional"}) || json_remove_string(text, {"host", "optional"}, name));
    ok = ok && (!json_has(text, {"host", "features"}) || json_remove_member(text, {"host", "features"}, name));
    if (is_private) {
        ok = ok && json_remove_member(text, {"packages"}, name);
        for (const auto& [var, target] : host.legacy_vars) {
            if (target == name + ".source_dir") ok = ok && json_remove_member(text, {"host", "legacy_vars"}, var);
        }
    }
    if (!ok) return edit_error("cannot rewrite the host declaration (unexpected JSON layout)");
    auto result = detail::finish_edit(host_text, std::move(text), ergo);
    if (result.ok && is_private) result.notes.push_back("removed the private package " + name);
    return result;
}

} // namespace ergo::package_manager
