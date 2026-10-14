// 依存の列挙・報告と出来事の書き手 (spec/feature/package-manager.md §7 W3: 報告と出来事が漏れ検査を通り、
// 通らない入力では書かない。 §8: 取得元の URL を報告に載せない)。

#include "fake_source.h"

#include "ergo/package_watch/observation.h"
#include "ergo/package_watch/time_text.h"
#include "ergo/package_watch/watch_dependency.h"
#include "ergo/package_watch/watch_report.h"

#include "gtest/gtest.h"

#include <fstream>
#include <sstream>

using namespace ergo::package_watch;
using ergo::package_watch::test::dependency;
using ergo::package_watch::test::FakeRepo;
using ergo::package_watch::test::FakeSource;
using ergo::package_watch::test::linear_main;
using ergo::package_watch::test::sha;

namespace jsonm = ergo::common::jsonm;
namespace pm    = ergo::package_manager;

namespace {

constexpr std::int64_t kT0 = 1792022400;  // 2026-10-15T00:00:00Z

jsonm::JsonValue fixture(const std::string& name) {
    std::ifstream in(std::string(ERGO_PACKAGE_WATCH_FIXTURES) + "/" + name, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    jsonm::JsonValue v;
    jsonm::parse(ss.str(), v);
    return v;
}

HostWatch watch_one(const WatchDependency& dep) {
    FakeSource src;
    FakeRepo& r = src.repos[dep.name];
    linear_main(r, 4);
    r.listing.tags["v1.1.0"] = sha(3);
    HostWatch w;
    w.host = "Example";
    w.results.push_back(make_result(dep, observe_dependency(dep, src, src, "")));
    w.diff = diff(WatchState{}, w.host, w.results, kT0);
    return w;
}

} // namespace

TEST(PackageWatchTime, UtcRoundTrip) {
    EXPECT_EQ(format_utc(0), std::string("1970-01-01T00:00:00Z"));
    EXPECT_EQ(format_utc(kT0), std::string("2026-10-15T00:00:00Z"));
    std::int64_t t = 0;
    ASSERT_TRUE(parse_utc("2026-10-15T09:30:05Z", t));
    EXPECT_EQ(t, kT0 + 9 * 3600 + 30 * 60 + 5);
    EXPECT_EQ(format_utc(t), std::string("2026-10-15T09:30:05Z"));
    EXPECT_FALSE(parse_utc("2026-10-15 09:30:05", t));
}

TEST(PackageWatchDependencies, FromHostDeclaration) {
    pm::HostDeclaration host;
    std::vector<pm::Finding> read;
    ASSERT_TRUE(pm::read_host_declaration(fixture("host.json"), host, read));
    pm::ErgoManifest ergo;
    std::string error;
    ASSERT_TRUE(pm::read_ergo_manifest(fixture("ergo-manifest.json"), ergo, &error));
    std::vector<std::string> skipped;
    const auto deps = host_dependencies(host, &ergo, &skipped);
    ASSERT_EQ(deps.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(deps[0].name, std::string("ergo"));
    EXPECT_EQ(deps[0].owner, std::string("host"));
    EXPECT_EQ(deps[0].track_branch, std::string("main"));
    EXPECT_EQ(deps[1].name, std::string("pictor"));
    EXPECT_EQ(deps[1].owner, std::string("ergo"));
    EXPECT_EQ(deps[1].expect_tag, std::string("v2.2.0"));
    EXPECT_EQ(deps[2].name, std::string("privlib"));
    EXPECT_TRUE(deps[2].is_private);
    EXPECT_EQ(deps[2].track_branch, std::string("release"));
    ASSERT_EQ(skipped.size(), static_cast<std::size_t>(1));  // nope: not in the Ergo manifest
    EXPECT_EQ(repository_name("https://github.com/LUDIARS/Pictor.git"), std::string("Pictor"));
    EXPECT_EQ(repository_name("git@example.invalid:team/Privlib"), std::string("Privlib"));

    const auto own = ergo_dependencies(ergo, &skipped);
    ASSERT_EQ(own.size(), static_cast<std::size_t>(2));  // pictor, tela (lapilli は tool)
    EXPECT_EQ(own[1].name, std::string("tela"));
}

TEST(PackageWatchReport, LeakFreeOutputsAreWritten) {
    const auto w = watch_one(dependency("privlib", 1, "v1.0.0"));
    ASSERT_EQ(w.diff.events.size(), static_cast<std::size_t>(2));
    const auto out = prepare_watch_outputs({w}, kT0);
    ASSERT_TRUE(out.ok);
    ASSERT_EQ(out.reports.size(), static_cast<std::size_t>(1));
    const std::string& report = out.reports[0].content;
    EXPECT_TRUE(report.find("\"Example/privlib/tag/v1.1.0\"") != std::string::npos);
    EXPECT_TRUE(report.find("\"private\":true") != std::string::npos);
    EXPECT_TRUE(report.find("\"pending\"") != std::string::npos);
    // 取得元の URL は報告にも出来事にも入らない (§8)。
    EXPECT_TRUE(report.find("example.invalid") == std::string::npos);
    EXPECT_TRUE(out.events.content.find("example.invalid") == std::string::npos);
    EXPECT_TRUE(out.events.content.find("\"hosts\":[\"Example\"]") != std::string::npos);
    EXPECT_EQ(report.back(), '\n');

    // 同じ入力なら同じバイト列。
    const auto again = prepare_watch_outputs({watch_one(dependency("privlib", 1, "v1.0.0"))}, kT0);
    EXPECT_EQ(again.reports[0].content, report);
    EXPECT_EQ(again.events.content, out.events.content);
}

TEST(PackageWatchReport, LeakBlocksAllOutputs) {
    // 依存の名前に開発機のパスが混じった (宣言の誤り): 報告も出来事も作らない。
    const auto w = watch_one(dependency("lib/home/someone", 1, "v1.0.0"));
    const auto out = prepare_watch_outputs({w}, kT0);
    EXPECT_FALSE(out.ok);
    EXPECT_FALSE(out.leaks.empty());
    EXPECT_TRUE(out.reports.empty());
    EXPECT_TRUE(out.events.content.empty());
    EXPECT_TRUE(out.leaks[0].where.rfind("Example:", 0) == 0);
}

TEST(PackageWatchReport, OutdatedTextAndJson) {
    const auto dep = dependency("privlib", 1, "v1.0.0");
    FakeSource src;  // privlib は取れない
    const std::vector<DependencyResult> results = {make_result(dep, observe_dependency(dep, src, src, ""))};
    const std::string text = render_outdated_text("Example", results);
    EXPECT_TRUE(text.find("unobserved: remote unreachable") != std::string::npos);
    const std::string json = jsonm::serialize(outdated_json("Example", results, kT0));
    EXPECT_TRUE(json.find("\"observed\":false") != std::string::npos);
    EXPECT_TRUE(json.find("\"updates\":[]") != std::string::npos);
}
