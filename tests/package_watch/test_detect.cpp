// 観測と判定 (spec/feature/package-manager.md §7 W3: tag / track / off-main の判定 (距離・semver・
// 複数タグの最新 1 つ)、 取得失敗で observed:false、 兄弟では fetch せず as_of が付く、
// 指紋が同じならミラーを fetch しない)。

#include "fake_source.h"

#include "ergo/package_watch/detect.h"
#include "ergo/package_watch/observation.h"
#include "ergo/package_watch/ref_listing.h"
#include "ergo/package_watch/semver.h"

#include "gtest/gtest.h"

using namespace ergo::package_watch;
using ergo::package_watch::test::dependency;
using ergo::package_watch::test::FakeRepo;
using ergo::package_watch::test::FakeSource;
using ergo::package_watch::test::linear_main;
using ergo::package_watch::test::sha;

namespace {

/// main = 1..5。 タグ: v1.0.0=1、 v1.1.0=3、 v1.2.0-rc.1=4 (pre-release)、
/// v2.0.0=100 (別の根 99 から伸びた枝。 main とも pin とも祖先関係が無い)。
FakeSource standard_source() {
    FakeSource src;
    FakeRepo& r = src.repos["lib"];
    linear_main(r, 5);
    r.parent[sha(100)]            = sha(99);
    r.listing.tags["v1.0.0"]      = sha(1);
    r.listing.tags["v1.1.0"]      = sha(3);
    r.listing.tags["v1.2.0-rc.1"] = sha(4);
    r.listing.tags["v2.0.0"]      = sha(100);
    r.listing.tags["release-9"]   = sha(5);  // v* でないものは見ない
    return src;
}

const Candidate* find_kind(const std::vector<Candidate>& cs, UpdateKind k) {
    for (const auto& c : cs) {
        if (c.kind == k) return &c;
    }
    return nullptr;
}

} // namespace

TEST(PackageWatchSemver, ParsesAndOrders) {
    ASSERT_TRUE(parse_semver("v1.10.0").has_value());
    EXPECT_FALSE(parse_semver("1.0.0").has_value());
    EXPECT_FALSE(parse_semver("vnext").has_value());
    EXPECT_FALSE(parse_semver("v1.2.3.4").has_value());
    EXPECT_GT(compare_semver(*parse_semver("v1.10.0"), *parse_semver("v1.9.0")), 0);
    EXPECT_EQ(compare_semver(*parse_semver("v1.2"), *parse_semver("v1.2.0")), 0);
    EXPECT_LT(compare_semver(*parse_semver("v1.2.0-rc.1"), *parse_semver("v1.2.0")), 0);
    EXPECT_LT(compare_semver(*parse_semver("v1.2.0-rc.2"), *parse_semver("v1.2.0-rc.10")), 0);
}

TEST(PackageWatchRefListing, ParsesLsRemoteAndPeelsTags) {
    const std::string text = sha(5) + "\trefs/heads/main\n" + sha(6) + "\trefs/heads/feat/x\n" +
                             sha(70) + "\trefs/tags/v1.1.0\n" + sha(3) + "\trefs/tags/v1.1.0^{}\n" +
                             sha(1) + "\trefs/tags/v1.0.0\n" + "garbage line\n" + sha(9) + "\trefs/pull/1/head\n";
    RefListing l;
    parse_ref_lines(text, RefLayout::Remote, l);
    EXPECT_EQ(l.heads.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(l.heads["main"], sha(5));
    EXPECT_EQ(l.tags["v1.1.0"], sha(3));  // 注釈つきタグは commit を見る
    EXPECT_EQ(l.tags["v1.0.0"], sha(1));
    EXPECT_EQ(l.tags.size(), static_cast<std::size_t>(2));

    RefListing sib;
    parse_ref_lines(sha(5) + " refs/remotes/origin/main\n" + sha(5) + " refs/remotes/origin/HEAD\n" +
                        sha(4) + " refs/heads/main\n",
                    RefLayout::Sibling, sib);
    EXPECT_EQ(sib.heads.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(sib.heads["main"], sha(5));
}

TEST(PackageWatchRefListing, FingerprintFollowsTheListing) {
    RefListing a;
    a.heads["main"] = sha(5);
    a.tags["v1.0.0"] = sha(1);
    RefListing b = a;
    EXPECT_EQ(listing_fingerprint(a), listing_fingerprint(b));
    EXPECT_EQ(listing_fingerprint(a).size(), static_cast<std::size_t>(16));
    b.heads["main"] = sha(6);
    EXPECT_NE(listing_fingerprint(a), listing_fingerprint(b));
}

TEST(PackageWatchDetect, TagAndTrackWithDistance) {
    auto src = standard_source();
    const auto dep = dependency("lib", 1, "v1.0.0");
    const auto obs = observe_dependency(dep, src, src, "");
    ASSERT_TRUE(obs.observed);
    EXPECT_EQ(obs.as_of, std::string(kAsOfRemote));
    const auto cs = detect_updates(dep, obs);
    ASSERT_EQ(cs.size(), static_cast<std::size_t>(2));

    // 複数の新しいタグのうち、 pin の子孫の最新 1 つ (v2.0.0 は横枝、 rc は pre-release)。
    const auto* tag = find_kind(cs, UpdateKind::Tag);
    ASSERT_TRUE(tag != nullptr);
    EXPECT_EQ(tag->subject, std::string("v1.1.0"));
    EXPECT_EQ(tag->to_rev, sha(3));
    EXPECT_EQ(tag->behind, 2);
    EXPECT_TRUE(tag->strict_ok);
    EXPECT_EQ(tag->next, std::string("ergo_pkg update lib --to v1.1.0"));

    const auto* track = find_kind(cs, UpdateKind::Track);
    ASSERT_TRUE(track != nullptr);
    EXPECT_EQ(track->subject, sha(5));
    EXPECT_EQ(track->behind, 4);
    EXPECT_FALSE(track->strict_ok);
    EXPECT_TRUE(find_kind(cs, UpdateKind::OffMain) == nullptr);
}

TEST(PackageWatchDetect, ExpectTagLimitsTags) {
    auto src = standard_source();
    // expect_tag が最新の正式版 (pin 3 = v1.1.0) なら tag は出ない。 track だけ。
    const auto dep = dependency("lib", 3, "v1.1.0");
    const auto cs = detect_updates(dep, observe_dependency(dep, src, src, ""));
    ASSERT_EQ(cs.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(cs[0].kind == UpdateKind::Track);
    EXPECT_EQ(cs[0].behind, 2);
}

TEST(PackageWatchDetect, OffMain) {
    auto src = standard_source();
    // pin が横枝 (main の祖先でない): off-main の警告だけ。 v2.0.0 はその pin 自身なので tag にならない。
    const auto dep = dependency("lib", 100, "v2.0.0");
    const auto cs = detect_updates(dep, observe_dependency(dep, src, src, ""));
    ASSERT_EQ(cs.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(cs[0].kind == UpdateKind::OffMain);
    EXPECT_EQ(cs[0].subject, sha(100));
    EXPECT_TRUE(cs[0].warning);
    EXPECT_FALSE(cs[0].behind.has_value());
}

TEST(PackageWatchDetect, UpToDate) {
    auto src = standard_source();
    const auto dep = dependency("lib", 5, "");
    const auto obs = observe_dependency(dep, src, src, "");
    EXPECT_EQ(obs.behind, 0);
    EXPECT_TRUE(detect_updates(dep, obs).empty());
}

TEST(PackageWatchDetect, WithoutHistoryDistanceIsBlank) {
    auto src = standard_source();
    src.repos["lib"].has_history = false;
    const auto dep = dependency("lib", 1, "v1.0.0");
    const auto cs = detect_updates(dep, observe_dependency(dep, src, src, ""));
    ASSERT_EQ(cs.size(), static_cast<std::size_t>(2));
    // 祖先が判らないので semver だけで選ぶ (最新の正式版 v2.0.0)。 距離は空欄。
    const auto* tag = find_kind(cs, UpdateKind::Tag);
    ASSERT_TRUE(tag != nullptr);
    EXPECT_EQ(tag->subject, std::string("v2.0.0"));
    EXPECT_FALSE(tag->behind.has_value());
    const auto* track = find_kind(cs, UpdateKind::Track);
    ASSERT_TRUE(track != nullptr);
    EXPECT_FALSE(track->behind.has_value());

    // expect_tag が無ければ履歴無しではタグを判定できない。
    const auto bare = dependency("lib", 1, "");
    const auto cs2 = detect_updates(bare, observe_dependency(bare, src, src, ""));
    EXPECT_TRUE(find_kind(cs2, UpdateKind::Tag) == nullptr);
}

TEST(PackageWatchObserve, FailureIsNotObserved) {
    FakeSource src;
    const auto dep = dependency("gone", 1, "");
    const auto obs = observe_dependency(dep, src, src, "");
    EXPECT_FALSE(obs.observed);
    EXPECT_EQ(obs.reason, std::string("remote unreachable"));
    EXPECT_TRUE(detect_updates(dep, obs).empty());
    EXPECT_EQ(src.sync_calls, 0);

    auto src2 = standard_source();
    auto dep2 = dependency("lib", 1, "");
    dep2.track_branch = "develop";
    const auto obs2 = observe_dependency(dep2, src2, src2, "");
    EXPECT_FALSE(obs2.observed);
    EXPECT_TRUE(obs2.reason.find("develop") != std::string::npos);
}

TEST(PackageWatchObserve, SameFingerprintSkipsSync) {
    auto src = standard_source();
    const auto dep = dependency("lib", 1, "v1.0.0");
    const auto first = observe_dependency(dep, src, src, "");
    EXPECT_EQ(src.sync_calls, 1);
    const auto second = observe_dependency(dep, src, src, first.fingerprint);
    EXPECT_EQ(src.sync_calls, 1);  // 指紋が同じ: ミラーを同期しない
    EXPECT_EQ(second.behind, first.behind);
    src.repos["lib"].listing.heads["main"] = sha(5);
    src.repos["lib"].parent[sha(6)]        = sha(5);
    src.repos["lib"].listing.heads["main"] = sha(6);
    observe_dependency(dep, src, src, first.fingerprint);
    EXPECT_EQ(src.sync_calls, 2);  // 先頭が動いた: 同期する
}

TEST(PackageWatchObserve, SiblingDoesNotFetch) {
    auto src = standard_source();
    src.repos["lib"].listing.origin     = RefOrigin::Sibling;
    src.repos["lib"].listing.local_head = sha(3);
    const auto dep = dependency("lib", 1, "v1.0.0");
    const auto obs = observe_dependency(dep, src, src, "");
    ASSERT_TRUE(obs.observed);
    EXPECT_EQ(src.sync_calls, 0);
    EXPECT_EQ(obs.as_of, std::string(kAsOfSibling));
    EXPECT_EQ(obs.sibling_behind, 2);
    EXPECT_EQ(obs.behind, 4);
}
