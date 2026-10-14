// plan_add — host.use に足す。 Ergo のマニフェストに無い名前は非公開 packages にも足す。

#include "ergo/package_manager/declaration_check.h"
#include "ergo/package_manager/declaration_edit.h"
#include "ergo/package_manager/package_owner.h"

#include "declaration_edit_support.h"

#include <cctype>

namespace ergo::package_manager {

namespace {

using detail::edit_error;

bool valid_name(const std::string& name) {
    if (name.empty() || !std::islower(static_cast<unsigned char>(name[0]))) return false;
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (!(std::islower(u) || std::isdigit(u) || c == '-' || c == '_')) return false;
    }
    return true;
}

bool has_private_fields(const AddRequest& r) {
    return !r.kind.empty() || !r.code.empty() || !r.repository.empty() || !r.rev.empty() || !r.wiring.empty();
}

/// 新しい非公開パッケージの packages.<name> を足す。
EditResult add_private_package(std::string& text, const AddRequest& r) {
    const std::string kind = r.kind.empty() ? "link" : r.kind;
    if (kind != "link" && kind != "tool") return edit_error("--kind must be link or tool");
    if (r.repository.empty() || r.rev.empty()) {
        return edit_error("'" + r.name + "' is not in the Ergo manifest: a private package needs --repository and --rev");
    }
    if (!is_full_sha(r.rev)) return edit_error("the resolved rev is not a 40-hex SHA: " + r.rev);
    const std::string wiring = r.wiring.empty() ? default_wiring_path(r.name) : r.wiring;
    if (kind == "link" && !r.wiring_exists) {
        return edit_error("the wiring file " + wiring +
                          " does not exist in the host (write it first; ergo_pkg does not generate it)");
    }
    std::vector<std::pair<std::string, std::string>> fields = {{"kind", json_quote(kind)}};
    if (!r.code.empty()) fields.emplace_back("code", json_quote(r.code));
    fields.emplace_back("repository", json_quote(r.repository));
    fields.emplace_back("rev", json_quote(r.rev));
    if (!r.expect_tag.empty()) fields.emplace_back("expect_tag", json_quote(r.expect_tag));
    if (kind == "link") fields.emplace_back("cmake", "{ \"wiring\": " + json_quote(wiring) + " }");
    if (!json_has(text, {"packages"}) && !json_set_member(text, {}, "packages", "{}")) {
        return edit_error("cannot add the `packages` section");
    }
    if (!json_add_object_member(text, {"packages"}, r.name, fields)) {
        return edit_error("cannot add packages." + r.name + " (unexpected JSON layout)");
    }
    EditResult ok;
    ok.notes.push_back("added the private package " + r.name + " at " + r.rev);
    return ok;
}

bool ensure_array(std::string& text, const char* key) {
    return json_has(text, {"host", key}) || json_set_member(text, {"host"}, key, "[]");
}

} // namespace

EditResult plan_add(const std::string& host_text, const ErgoManifest* ergo, const AddRequest& r) {
    if (!valid_name(r.name)) return edit_error("'" + r.name + "' is not a package name (lower case, digits, - and _)");
    if (r.name == kErgoPackageName) return edit_error("Ergo itself is pinned by host.ergo; use `update ergo`");
    HostDeclaration host;
    std::string error;
    if (!detail::parse_host_text(host_text, host, error)) return edit_error(error);

    std::string text = host_text;
    std::vector<std::string> notes;
    switch (owner_of(r.name, &host, ergo)) {
        case PackageOwner::Conflict:
            return edit_error("'" + r.name + "' is both private and in the Ergo manifest (PKG-15)");
        case PackageOwner::ErgoShared:
        case PackageOwner::HostPrivate:
            if (has_private_fields(r)) {
                return edit_error("'" + r.name + "' is already declared (owner: " +
                                  owner_name(owner_of(r.name, &host, ergo)) +
                                  "); --kind/--code/--repository/--rev/--wiring are only for a new private package");
            }
            break;
        case PackageOwner::Unknown:
            if (!ergo) return edit_error("the Ergo manifest is needed to tell a shared package from a private one (pass --ergo)");
            if (ergo->has_name(r.name)) {
                return edit_error("'" + r.name + "' is an Ergo module or observed repository, not a package");
            }
            {
                auto added = add_private_package(text, r);
                if (!added.ok) return added;
                notes = added.notes;
            }
            break;
        case PackageOwner::HostErgo:
            break;
    }

    bool ok = ensure_array(text, "use") && json_append_string(text, {"host", "use"}, r.name);
    if (r.optional) ok = ok && ensure_array(text, "optional") && json_append_string(text, {"host", "optional"}, r.name);
    if (!r.features.empty()) {
        ok = ok && (json_has(text, {"host", "features"}) || json_set_member(text, {"host"}, "features", "{}"));
        ok = ok && json_set_member(text, {"host", "features"}, r.name, json_quote(r.features));
    }
    if (!ok) return edit_error("cannot rewrite host.use / optional / features (unexpected JSON layout)");

    auto result = detail::finish_edit(host_text, std::move(text), ergo);
    if (result.ok) result.notes.insert(result.notes.begin(), notes.begin(), notes.end());
    return result;
}

} // namespace ergo::package_manager
