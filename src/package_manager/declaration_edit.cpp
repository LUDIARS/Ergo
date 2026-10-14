#include "ergo/package_manager/declaration_edit.h"

#include "declaration_edit_support.h"

#include "ergo/common/json_min.h"
#include "ergo/package_manager/declaration_check.h"

#include <algorithm>
#include <set>

namespace ergo::package_manager {

namespace detail {

EditResult edit_error(const std::string& why) {
    EditResult r;
    r.ok    = false;
    r.error = why;
    return r;
}

bool parse_host_text(const std::string& host_text, HostDeclaration& out, std::string& error) {
    ergo::common::jsonm::JsonValue doc;
    if (!ergo::common::jsonm::parse(host_text, doc)) {
        error = "the host declaration is not valid JSON";
        return false;
    }
    if (!is_host_declaration(doc)) {
        error = "ergo-packages.json has no `host` section (run `ergo_pkg init` in the host first)";
        return false;
    }
    std::vector<Finding> ignored;
    out = HostDeclaration{};
    return read_host_declaration(doc, out, ignored);
}

namespace {

std::set<std::string> errors_of(const std::string& text, const ErgoManifest* ergo) {
    std::set<std::string> out;
    ergo::common::jsonm::JsonValue doc;
    if (!ergo::common::jsonm::parse(text, doc)) return {"MANIFEST|error|ergo-packages.json|not valid JSON"};
    HostDeclaration host;
    std::vector<Finding> findings;
    read_host_declaration(doc, host, findings);
    for (const auto& f : check_declaration(host, ergo)) findings.push_back(f);
    for (const auto& f : findings) {
        if (f.severity == Severity::Error) out.insert(describe(f));
    }
    return out;
}

} // namespace

EditResult finish_edit(const std::string& before, std::string after, const ErgoManifest* ergo) {
    const auto old_errors = errors_of(before, ergo);
    std::string introduced;
    for (const auto& e : errors_of(after, ergo)) {
        if (!old_errors.count(e)) introduced += "\n  " + e;
    }
    if (!introduced.empty()) return edit_error("the change would make the declaration invalid:" + introduced);
    EditResult r;
    r.changed = after != before;
    r.text    = std::move(after);
    return r;
}

bool contains(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

} // namespace detail

std::string default_wiring_path(const std::string& name) { return "cmake/packages/" + name + ".cmake"; }

EditResult plan_pin(const std::string& owner_text, const JsonPath& pin_object, const std::string& rev,
                    const std::string& expect_tag) {
    if (!is_full_sha(rev)) return detail::edit_error("the resolved rev is not a 40-hex SHA: " + rev);
    std::string text = owner_text;
    if (!json_has(text, pin_object)) return detail::edit_error("the pin object is missing from the file");
    bool ok = json_set_member(text, pin_object, "rev", json_quote(rev));
    if (expect_tag.empty()) ok = ok && json_remove_member(text, pin_object, "expect_tag");
    else ok = ok && json_set_member(text, pin_object, "expect_tag", json_quote(expect_tag));
    if (!ok) return detail::edit_error("cannot rewrite the pin (unexpected JSON layout)");
    EditResult r;
    r.changed = text != owner_text;
    r.text    = std::move(text);
    return r;
}

} // namespace ergo::package_manager
