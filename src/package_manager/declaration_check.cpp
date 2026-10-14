#include "ergo/package_manager/declaration_check.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <set>

namespace ergo::package_manager {

namespace {

void add(std::vector<Finding>& out, const char* id, Severity s, const std::string& subject,
         const std::string& message) {
    out.push_back({id, s, subject, message});
}

bool contains(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

const ManifestPackage* find_private(const HostDeclaration& host, const std::string& name) {
    for (const auto& p : host.packages) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

/// 宣言から使える link パッケージか (公開: Ergo のマニフェスト、 非公開: ホスト)。
/// Ergo のマニフェストが無いときは公開名を判定できないので知っているものとして扱う。
bool known_link(const HostDeclaration& host, const ErgoManifest* ergo, const std::string& name) {
    if (const auto* p = find_private(host, name)) return p->kind == "link";
    if (!ergo) return true;
    const auto* p = ergo->find(name);
    return p && p->kind == "link";
}

bool is_identifier(const std::string& s) {
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    });
}

/// "+vulkan,-web" / "vulkan;web" の形か (ERGO_PACKAGE_FEATURES_<NAME> の既定値)。
bool valid_feature_request(const std::string& value) {
    std::string token;
    auto token_ok = [](const std::string& t) {
        std::size_t i = (!t.empty() && (t[0] == '+' || t[0] == '-')) ? 1 : 0;
        if (i >= t.size()) return false;
        return std::all_of(t.begin() + static_cast<std::ptrdiff_t>(i), t.end(), [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
        });
    };
    for (std::size_t i = 0; i <= value.size(); ++i) {
        if (i == value.size() || value[i] == ',' || value[i] == ';') {
            if (!token_ok(token)) return false;
            token.clear();
        } else {
            token += value[i];
        }
    }
    return true;
}

void check_ergo_pin(const HostDeclaration& host, std::vector<Finding>& out) {
    if (!host.ergo.present) {
        add(out, "MANIFEST", Severity::Warning, "ergo",
            "host.ergo is missing: the Ergo this host builds on is not pinned");
        return;
    }
    if (host.ergo.repository.empty()) {
        add(out, "MANIFEST", Severity::Error, "ergo", "host.ergo.repository is empty");
    }
    if (!is_full_sha(host.ergo.rev)) {
        add(out, "PKG-02", Severity::Error, "ergo",
            "host.ergo.rev '" + host.ergo.rev + "' is not a 40-digit lowercase commit SHA");
    }
}

void check_private_packages(const HostDeclaration& host, const ErgoManifest* ergo,
                            std::vector<Finding>& out) {
    for (const auto& p : host.packages) {
        if (ergo && ergo->has_name(p.name)) {
            add(out, "PKG-15", Severity::Error, p.name,
                "host declaration reuses a name of the Ergo manifest (private packages must be new names)");
        }
        if (p.kind != "link" && p.kind != "tool") {
            add(out, "MANIFEST", Severity::Error, p.name,
                "private package kind '" + p.kind + "' (want link or tool)");
            continue;
        }
        if (p.kind == "link") {
            if (p.repository.empty()) {
                add(out, "MANIFEST", Severity::Error, p.name, "private link package has no repository");
            }
            if (!is_full_sha(p.rev)) {
                add(out, "PKG-02", Severity::Error, p.name,
                    "rev '" + p.rev + "' is not a 40-digit lowercase commit SHA");
            }
        }
    }
}

void check_selection(const HostDeclaration& host, const ErgoManifest* ergo,
                     std::vector<Finding>& out) {
    std::set<std::string> seen;
    for (const auto& n : host.use) {
        if (!seen.insert(n).second) {
            add(out, "MANIFEST", Severity::Warning, n, "host.use lists '" + n + "' twice");
        }
        if (!known_link(host, ergo, n)) {
            add(out, "PKG-01", Severity::Error, n, "host.use names an unknown link package");
        }
    }
    for (const auto& n : host.optional) {
        if (!known_link(host, ergo, n)) {
            add(out, "PKG-01", Severity::Error, n, "host.optional names an unknown link package");
        } else if (!contains(host.use, n)) {
            add(out, "MANIFEST", Severity::Warning, n,
                "host.optional lists '" + n + "' which is not in host.use");
        }
    }
    for (const auto& [n, value] : host.features) {
        if (!known_link(host, ergo, n)) {
            add(out, "PKG-01", Severity::Error, n, "host.features names an unknown link package");
        }
        if (!valid_feature_request(value)) {
            add(out, "MANIFEST", Severity::Error, n,
                "host.features value '" + value + "' is not a feature request (e.g. +vulkan,-web)");
        }
    }
}

void check_legacy_vars(const HostDeclaration& host, const ErgoManifest* ergo,
                       std::vector<Finding>& out) {
    const std::string suffix = ".source_dir";
    for (const auto& [var, target] : host.legacy_vars) {
        if (!is_identifier(var)) {
            add(out, "MANIFEST", Severity::Error, var, "host.legacy_vars key is not a variable name");
        }
        bool ok = false;
        if (target.size() > suffix.size() &&
            target.compare(target.size() - suffix.size(), suffix.size(), suffix) == 0) {
            const std::string pkg = target.substr(0, target.size() - suffix.size());
            ok = pkg == "ergo" || known_link(host, ergo, pkg);
        }
        if (!ok) {
            add(out, "MANIFEST", Severity::Error, var,
                "host.legacy_vars target '" + target + "' (want ergo.source_dir or <package>.source_dir)");
        }
    }
}

} // namespace

bool is_full_sha(const std::string& rev) {
    return rev.size() == 40 && std::all_of(rev.begin(), rev.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

std::vector<Finding> check_declaration(const HostDeclaration& host, const ErgoManifest* ergo) {
    std::vector<Finding> out;
    if (host.code.empty()) {
        add(out, "MANIFEST", Severity::Record, "host",
            "host.code is empty: resolved.json takes ERGO_PACKAGES_HOST_ID or the project name");
    }
    check_ergo_pin(host, out);
    check_private_packages(host, ergo, out);
    check_selection(host, ergo, out);
    check_legacy_vars(host, ergo, out);
    sort_findings(out);
    return out;
}

} // namespace ergo::package_manager
