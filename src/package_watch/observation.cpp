#include "ergo/package_watch/observation.h"

#include "ergo/package_watch/ref_listing.h"
#include "ergo/package_watch/semver.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace ergo::package_watch {

const char* const kAsOfRemote  = "ls-remote";
const char* const kAsOfSibling = "sibling-last-fetch";

namespace {

/// 正式版の v* タグを semver の新しい順に (同じ版は名前順)。 pre-release ("v1.2.0-rc.1") は知らせない。
std::vector<std::pair<Semver, std::string>> versioned_tags(const RefListing& listing) {
    std::vector<std::pair<Semver, std::string>> out;
    for (const auto& kv : listing.tags) {
        auto v = parse_semver(kv.first);
        if (v && v->pre.empty()) out.emplace_back(std::move(*v), kv.first);
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return compare_semver(a.first, b.first) > 0;
    });
    return out;
}

void observe_track(const WatchDependency& dep, const RepoRef& repo, IHistorySource& history,
                   Observation& o) {
    if (o.track_head == dep.rev) {
        o.pin_on_track = true;
        o.behind       = 0;
        return;
    }
    o.pin_on_track = history.is_ancestor(repo, dep.rev, o.track_head);
    if (o.pin_on_track.value_or(false)) o.behind = history.count_commits(repo, dep.rev, o.track_head);
}

void observe_sibling(const RefListing& listing, const RepoRef& repo, IHistorySource& history,
                     Observation& o) {
    if (listing.local_head.empty()) return;
    if (listing.local_head == o.track_head) {
        o.sibling_behind = 0;
        return;
    }
    if (history.is_ancestor(repo, listing.local_head, o.track_head).value_or(false)) {
        o.sibling_behind = history.count_commits(repo, listing.local_head, o.track_head);
    }
}

void observe_tags(const WatchDependency& dep, const RefListing& listing, const RepoRef& repo,
                  IHistorySource& history, Observation& o) {
    const auto expect = parse_semver(dep.expect_tag);
    for (const auto& [version, name] : versioned_tags(listing)) {
        if (expect && compare_semver(version, *expect) <= 0) break;
        const std::string& rev = listing.tags.at(name);
        if (rev == dep.rev) break;  // pin そのもののタグ。 これより古いものは見ない
        const auto descends = history.is_ancestor(repo, dep.rev, rev);
        if (!descends) {
            // 履歴が答えないときは expect_tag より新しいことだけで選ぶ。 expect_tag が無ければ判定できない。
            if (expect) o.latest_tag = TagObservation{name, rev, std::nullopt};
            return;
        }
        if (*descends) {
            o.latest_tag = TagObservation{name, rev, history.count_commits(repo, dep.rev, rev)};
            return;
        }
        // pin より古いタグに着いたら、 それより古い版も pin の子孫ではない。
        if (history.is_ancestor(repo, rev, dep.rev).value_or(false)) return;
    }
}

} // namespace

Observation observe_dependency(const WatchDependency& dep, IRefSource& refs,
                               IHistorySource& history, const std::string& previous_fingerprint) {
    Observation o;
    const RepoRef repo{dep.name, dep.repository};
    const RefListing listing = refs.list_refs(repo);
    if (!listing.observed) {
        o.reason = listing.reason.empty() ? std::string("refs could not be listed") : listing.reason;
        return o;
    }
    const bool sibling = listing.origin == RefOrigin::Sibling;
    o.as_of       = sibling ? kAsOfSibling : kAsOfRemote;
    o.fingerprint = listing_fingerprint(listing);
    const auto head = listing.heads.find(dep.track_branch);
    if (head == listing.heads.end()) {
        o.reason = "track branch '" + dep.track_branch + "' not found";
        return o;
    }
    o.observed   = true;
    o.track_head = head->second;
    // 指紋が同じならミラーは前回の同期で足りている。 兄弟チェックアウトは fetch しない。
    if (!sibling && o.fingerprint != previous_fingerprint) history.sync_history(repo);

    observe_track(dep, repo, history, o);
    if (sibling) observe_sibling(listing, repo, history, o);
    observe_tags(dep, listing, repo, history, o);
    return o;
}

} // namespace ergo::package_watch
