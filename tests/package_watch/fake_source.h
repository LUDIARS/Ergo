#pragma once

// test_package_watch の偽ソース: ネットワークも git も使わずに IRefSource / IHistorySource を演じる。
// 履歴は 1 親の連鎖 (sha → 親) で持ち、 呼び出し回数を数える。

#include "ergo/package_watch/ref_source.h"
#include "ergo/package_watch/watch_dependency.h"

#include <cstdio>
#include <map>
#include <optional>
#include <string>

namespace ergo::package_watch::test {

/// n から作る 40 桁の SHA。
inline std::string sha(int n) {
    char buf[41];
    std::snprintf(buf, sizeof(buf), "%040x", static_cast<unsigned>(n));
    return buf;
}

struct FakeRepo {
    RefListing                         listing;
    std::map<std::string, std::string> parent;  ///< commit → 親 (無ければ根)
    bool                               has_history = true;
};

class FakeSource : public IRefSource, public IHistorySource {
public:
    std::map<std::string, FakeRepo> repos;  ///< 依存名 → リポ
    int list_calls  = 0;
    int sync_calls  = 0;
    int query_calls = 0;

    RefListing list_refs(const RepoRef& repo) override {
        ++list_calls;
        const auto it = repos.find(repo.name);
        if (it == repos.end()) {
            RefListing l;
            l.reason = "remote unreachable";
            return l;
        }
        return it->second.listing;
    }

    bool sync_history(const RepoRef&) override {
        ++sync_calls;
        return true;
    }

    std::optional<int> count_commits(const RepoRef& repo, const std::string& from,
                                     const std::string& to) override {
        ++query_calls;
        const FakeRepo* r = find(repo);
        if (!r) return std::nullopt;
        int n = 0;
        for (std::string at = to; !at.empty(); at = parent_of(*r, at), ++n) {
            if (at == from) return n;
        }
        return std::nullopt;
    }

    std::optional<bool> is_ancestor(const RepoRef& repo, const std::string& ancestor,
                                    const std::string& descendant) override {
        ++query_calls;
        const FakeRepo* r = find(repo);
        if (!r) return std::nullopt;
        for (std::string at = descendant; !at.empty(); at = parent_of(*r, at)) {
            if (at == ancestor) return true;
        }
        return false;
    }

private:
    const FakeRepo* find(const RepoRef& repo) const {
        const auto it = repos.find(repo.name);
        if (it == repos.end() || !it->second.has_history) return nullptr;
        return &it->second;
    }

    static std::string parent_of(const FakeRepo& r, const std::string& c) {
        const auto it = r.parent.find(c);
        return it == r.parent.end() ? std::string() : it->second;
    }
};

/// 1 本の main: sha(1) ← sha(2) ← ... ← sha(head)。
inline void linear_main(FakeRepo& r, int head) {
    for (int i = 2; i <= head; ++i) r.parent[sha(i)] = sha(i - 1);
    r.listing.observed      = true;
    r.listing.heads["main"] = sha(head);
}

inline WatchDependency dependency(const std::string& name, int pin, const std::string& expect_tag) {
    WatchDependency d;
    d.name         = name;
    d.code         = "Ex";
    d.owner        = "host";
    d.repository   = "https://example.invalid/" + name + ".git";
    d.rev          = sha(pin);
    d.expect_tag   = expect_tag;
    d.track_branch = "main";
    d.is_private   = true;
    return d;
}

} // namespace ergo::package_watch::test
