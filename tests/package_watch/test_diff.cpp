// 前回状態との差分 (spec/feature/package-manager.md §7 W3: diff の決定性 (同じ入力で同じバイト列)、
// 同じ主体の 2 回目で出来事 0、 track の 6 時間畳み込み、 pin を上げると解消、 取得失敗で前回状態不変)。

#include "fake_source.h"

#include "ergo/package_watch/observation.h"
#include "ergo/package_watch/watch_diff.h"
#include "ergo/package_watch/watch_state.h"

#include "gtest/gtest.h"

#include <algorithm>

using namespace ergo::package_watch;
using ergo::package_watch::test::dependency;
using ergo::package_watch::test::FakeRepo;
using ergo::package_watch::test::FakeSource;
using ergo::package_watch::test::linear_main;
using ergo::package_watch::test::sha;

namespace jsonm = ergo::common::jsonm;

namespace {

constexpr std::int64_t kT0 = 1792022400;  // 2026-10-15T00:00:00Z

/// a: main = 1..5、 v1.1.0 = 3。 b: main = 1..3、 タグ無し。
FakeSource two_repos() {
    FakeSource src;
    FakeRepo& a = src.repos["a"];
    linear_main(a, 5);
    a.listing.tags["v1.0.0"] = sha(1);
    a.listing.tags["v1.1.0"] = sha(3);
    FakeRepo& b = src.repos["b"];
    linear_main(b, 3);
    return src;
}

std::vector<DependencyResult> observe_all(FakeSource& src, const std::vector<WatchDependency>& deps,
                                          const WatchState& prev) {
    std::vector<DependencyResult> out;
    for (const auto& d : deps) {
        const auto fp = prev.fingerprints.count(d.name) ? prev.fingerprints.at(d.name) : std::string();
        out.push_back(make_result(d, observe_dependency(d, src, src, fp)));
    }
    return out;
}

std::string bytes(const WatchState& s) { return jsonm::serialize(state_to_json(s)); }

std::vector<WatchDependency> deps_ab() {
    return {dependency("a", 1, "v1.0.0"), dependency("b", 1, "")};
}

} // namespace

TEST(PackageWatchDiff, FirstRunReportsEverything) {
    auto src = two_repos();
    const WatchState empty;
    const auto r = diff(empty, "Example", observe_all(src, deps_ab(), empty), kT0);
    ASSERT_EQ(r.events.size(), static_cast<std::size_t>(3));  // a: tag + track、 b: track
    EXPECT_EQ(r.events[0].id, std::string("Example/a/tag/v1.1.0"));
    EXPECT_EQ(r.events[1].id, "Example/a/track/" + sha(5));
    EXPECT_EQ(r.events[2].id, "Example/b/track/" + sha(3));
    EXPECT_EQ(r.next.entries.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(r.next.fingerprints.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(r.events[0].at, kT0);
}

TEST(PackageWatchDiff, Deterministic) {
    auto src = two_repos();
    const WatchState empty;
    auto results = observe_all(src, deps_ab(), empty);
    const auto r1 = diff(empty, "Example", results, kT0);
    std::reverse(results.begin(), results.end());
    const auto r2 = diff(empty, "Example", results, kT0);
    EXPECT_EQ(bytes(r1.next), bytes(r2.next));
    ASSERT_EQ(r1.events.size(), r2.events.size());
    for (std::size_t i = 0; i < r1.events.size(); ++i) EXPECT_EQ(r1.events[i].id, r2.events[i].id);
    // 状態は読み直しても同じバイト列。
    WatchState back;
    std::string error;
    ASSERT_TRUE(read_state(state_to_json(r1.next), back, &error));
    EXPECT_EQ(bytes(back), bytes(r1.next));
}

TEST(PackageWatchDiff, SecondRunHasNoEvents) {
    auto src = two_repos();
    const WatchState empty;
    const auto r1 = diff(empty, "Example", observe_all(src, deps_ab(), empty), kT0);
    const auto r2 = diff(r1.next, "Example", observe_all(src, deps_ab(), r1.next), kT0 + 900);
    EXPECT_TRUE(r2.events.empty());
    EXPECT_EQ(bytes(r2.next), bytes(r1.next));  // 最初に見た時刻も最後の通知も変わらない
}

TEST(PackageWatchDiff, TrackFoldsWithinSixHours) {
    auto src = two_repos();
    const std::vector<WatchDependency> deps = {dependency("b", 1, "")};
    const WatchState empty;
    const auto r1 = diff(empty, "Example", observe_all(src, deps, empty), kT0);
    ASSERT_EQ(r1.events.size(), static_cast<std::size_t>(1));  // track +2

    // 1 時間後に main が 1 つ進む: 畳む (出来事 0、 主体は新しい先頭、 最後の通知は kT0 のまま)。
    src.repos["b"].parent[sha(4)]        = sha(3);
    src.repos["b"].listing.heads["main"] = sha(4);
    const auto r2 = diff(r1.next, "Example", observe_all(src, deps, r1.next), kT0 + 3600);
    EXPECT_TRUE(r2.events.empty());
    ASSERT_EQ(r2.next.entries.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(r2.next.entries[0].update.subject, sha(4));
    EXPECT_EQ(r2.next.entries[0].last_notified, kT0);

    // 7 時間後にもう 1 つ進む: 出す。 前回の通知 (+2) からの差 +2 を添える。
    src.repos["b"].parent[sha(5)]        = sha(4);
    src.repos["b"].listing.heads["main"] = sha(5);
    const auto r3 = diff(r2.next, "Example", observe_all(src, deps, r2.next), kT0 + 7 * 3600);
    ASSERT_EQ(r3.events.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(r3.events[0].update.kind == UpdateKind::Track);
    EXPECT_EQ(r3.events[0].update.behind, 4);
    EXPECT_EQ(r3.events[0].since_notified, 2);
    EXPECT_EQ(r3.next.entries[0].last_notified, kT0 + 7 * 3600);
    EXPECT_EQ(r3.next.entries[0].notified_behind, 4);
}

TEST(PackageWatchDiff, RaisingThePinResolves) {
    auto src = two_repos();
    const WatchState empty;
    const auto r1 = diff(empty, "Example", observe_all(src, deps_ab(), empty), kT0);
    // a の pin を main の先頭へ上げた: a の既知は消える (v1.1.0 は pin の祖先、 track は差なし)。
    const std::vector<WatchDependency> raised = {dependency("a", 5, "v1.1.0"), dependency("b", 1, "")};
    const auto r2 = diff(r1.next, "Example", observe_all(src, raised, r1.next), kT0 + 60);
    EXPECT_TRUE(r2.events.empty());
    ASSERT_EQ(r2.next.entries.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(r2.next.entries[0].update.dependency, std::string("b"));

    // pin を戻すと再発は新規として出る。
    const auto r3 = diff(r2.next, "Example", observe_all(src, deps_ab(), r2.next), kT0 + 120);
    EXPECT_EQ(r3.events.size(), static_cast<std::size_t>(2));
}

TEST(PackageWatchDiff, UnobservedKeepsState) {
    auto src = two_repos();
    const WatchState empty;
    const auto r1 = diff(empty, "Example", observe_all(src, deps_ab(), empty), kT0);
    // a が取れなくなった (ネットワーク無し): 出来事 0、 a の既知と指紋は前回のまま。
    src.repos.erase("a");
    const auto results = observe_all(src, deps_ab(), r1.next);
    EXPECT_FALSE(results[0].observation.observed);
    const auto r2 = diff(r1.next, "Example", results, kT0 + 900);
    EXPECT_TRUE(r2.events.empty());
    EXPECT_EQ(bytes(r2.next), bytes(r1.next));
}

TEST(PackageWatchDiff, RemovedDependencyIsDropped) {
    auto src = two_repos();
    const WatchState empty;
    const auto r1 = diff(empty, "Example", observe_all(src, deps_ab(), empty), kT0);
    const std::vector<WatchDependency> only_b = {dependency("b", 1, "")};
    const auto r2 = diff(r1.next, "Example", observe_all(src, only_b, r1.next), kT0 + 60);
    EXPECT_EQ(r2.next.entries.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(r2.next.fingerprints.count("a"), static_cast<std::size_t>(0));
}

TEST(PackageWatchState, RejectsBadShapes) {
    WatchState s;
    std::string error;
    jsonm::JsonValue doc;
    jsonm::parse(R"({"schema":2})", doc);
    EXPECT_FALSE(read_state(doc, s, &error));
    jsonm::parse(R"({"schema":1,"entries":[{"dependency":"a","kind":"weird","subject":"x"}]})", doc);
    EXPECT_FALSE(read_state(doc, s, &error));
    jsonm::parse(R"({"schema":1,"entries":[{"dependency":"a","kind":"tag","subject":"v1",)"
                 R"("first_seen":"yesterday","last_notified":"2026-10-15T00:00:00Z"}]})",
                 doc);
    EXPECT_FALSE(read_state(doc, s, &error));
    jsonm::parse(R"({"schema":1,"host":"Example","fingerprints":{},"entries":[]})", doc);
    EXPECT_TRUE(read_state(doc, s, &error));
    EXPECT_EQ(s.host, std::string("Example"));
}
