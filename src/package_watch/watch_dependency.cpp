#include "ergo/package_watch/watch_dependency.h"

#include "ergo/package_manager/declaration_check.h"

#include <algorithm>

namespace ergo::package_watch {

const char* const kDefaultTrackBranch = "main";
const char* const kErgoDependency     = "ergo";
const char* const kErgoHostCode       = "Ergo";
const char* const kErgoCode           = "Eg";

namespace {

std::string branch_or_default(const std::string& branch) {
    return branch.empty() ? std::string(kDefaultTrackBranch) : branch;
}

/// 見られる (取得元と 40 桁 SHA の pin がある) なら out に足し、 違えば skipped に理由。
void add_if_watchable(WatchDependency dep, std::vector<WatchDependency>& out,
                      std::vector<std::string>* skipped) {
    if (dep.repository.empty()) {
        if (skipped) skipped->push_back(dep.name + ": no repository");
        return;
    }
    if (!pm::is_full_sha(dep.rev)) {
        if (skipped) skipped->push_back(dep.name + ": pin is not a 40-digit SHA");
        return;
    }
    out.push_back(std::move(dep));
}

WatchDependency from_package(const pm::ManifestPackage& p, const char* owner, bool is_private) {
    WatchDependency d;
    d.name         = p.name;
    d.code         = p.code;
    d.owner        = owner;
    d.repository   = p.repository;
    d.rev          = p.rev;
    d.expect_tag   = p.expect_tag;
    d.track_branch = branch_or_default(p.track_branch);
    d.is_private   = is_private;
    return d;
}

void sort_by_name(std::vector<WatchDependency>& deps) {
    std::sort(deps.begin(), deps.end(),
              [](const WatchDependency& a, const WatchDependency& b) { return a.name < b.name; });
}

} // namespace

std::string repository_name(const std::string& repository) {
    std::string s = repository;
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    const auto cut = s.find_last_of("/\\:");
    std::string tail = cut == std::string::npos ? s : s.substr(cut + 1);
    const std::string suffix = ".git";
    if (tail.size() > suffix.size() && tail.compare(tail.size() - suffix.size(), suffix.size(), suffix) == 0) {
        tail.erase(tail.size() - suffix.size());
    }
    return tail;
}

std::vector<WatchDependency> host_dependencies(const pm::HostDeclaration& host,
                                               const pm::ErgoManifest* ergo,
                                               std::vector<std::string>* skipped) {
    std::vector<WatchDependency> out;
    if (host.ergo.present) {
        WatchDependency d;
        d.name         = kErgoDependency;
        d.code         = kErgoCode;
        d.owner        = "host";
        d.repository   = host.ergo.repository;
        d.rev          = host.ergo.rev;
        d.expect_tag   = host.ergo.expect_tag;
        d.track_branch = branch_or_default(host.ergo.track_branch);
        add_if_watchable(std::move(d), out, skipped);
    }
    for (const auto& name : host.use) {
        const bool is_private = std::any_of(host.packages.begin(), host.packages.end(),
                                            [&](const pm::ManifestPackage& p) { return p.name == name; });
        if (is_private) continue;  // 下の非公開の列で足す
        const pm::ManifestPackage* p = ergo ? ergo->find(name) : nullptr;
        if (!p) {
            if (skipped) skipped->push_back(name + ": not in the Ergo manifest");
            continue;
        }
        if (p->kind != "link") continue;
        add_if_watchable(from_package(*p, "ergo", false), out, skipped);
    }
    for (const auto& p : host.packages) {
        if (p.kind != "link") continue;
        add_if_watchable(from_package(p, "host", true), out, skipped);
    }
    sort_by_name(out);
    return out;
}

std::vector<WatchDependency> ergo_dependencies(const pm::ErgoManifest& ergo,
                                               std::vector<std::string>* skipped) {
    std::vector<WatchDependency> out;
    for (const auto& p : ergo.packages) {
        if (p.kind != "link") continue;
        add_if_watchable(from_package(p, "ergo", false), out, skipped);
    }
    sort_by_name(out);
    return out;
}

} // namespace ergo::package_watch
