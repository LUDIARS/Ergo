// UPM ホスト (spec/feature/package-manager.md §6.3 / §7 W7): Unity プロジェクトの Packages/manifest.json の git 依存を
// 更新を確かめる依存にし、 偽ソースから出来事を出す。 Unity 側と共有する報告の形 (fixtures/watch-report.json) が
// 書き手の出力と同じキーを持つことも確かめる。

#include "fake_source.h"

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/upm_declaration.h"
#include "ergo/package_watch/observation.h"
#include "ergo/package_watch/upm_dependency.h"
#include "ergo/package_watch/watch_diff.h"
#include "ergo/package_watch/watch_report.h"

#include "gtest/gtest.h"

#include <fstream>
#include <set>
#include <sstream>

using namespace ergo::package_watch;
using ergo::package_watch::test::FakeRepo;
using ergo::package_watch::test::FakeSource;
using ergo::package_watch::test::sha;

namespace jsonm = ergo::common::jsonm;
namespace pm    = ergo::package_manager;

namespace {

constexpr std::int64_t kT0 = 1792022400;  // 2026-10-15T00:00:00Z

std::string fixture_text(const std::string& name) {
    std::ifstream in(std::string(ERGO_PACKAGE_WATCH_FIXTURES) + "/" + name, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

jsonm::JsonValue fixture(const std::string& name) {
    jsonm::JsonValue v;
    jsonm::parse(fixture_text(name), v);
    return v;
}

pm::UpmDeclaration fixture_upm() {
    pm::UpmDeclaration upm;
    upm.dependencies = pm::read_upm_dependencies(fixture("upm-manifest.json"));
    upm.code         = pm::read_upm_host_code(fixture("upm-host.json"));
    return upm;
}

pm::ErgoManifest fixture_ergo() {
    pm::ErgoManifest m;
    pm::read_ergo_manifest(fixture("ergo-manifest.json"), m, nullptr);
    return m;
}

const WatchDependency* find_dep(const std::vector<WatchDependency>& deps, const std::string& name) {
    for (const auto& d : deps) {
        if (d.name == name) return &d;
    }
    return nullptr;
}

/// UPM ホスト見本を偽ソースで見る: Ergo のパッケージは pin ← sha(2) (v0.10.0) ← sha(3) = main。
/// Pictor は取得元に無い (取れない)。
HostWatch watch_upm_fixture() {
    const auto upm  = fixture_upm();
    const auto ergo = fixture_ergo();
    const auto deps = upm_dependencies(upm, &ergo, nullptr);

    FakeSource src;
    const WatchDependency* pkg = find_dep(deps, "com.ludiars.ergo.packages");
    if (pkg) {
        FakeRepo& r                   = src.repos[pkg->name];
        r.parent[sha(2)]              = pkg->rev;
        r.parent[sha(3)]              = sha(2);
        r.listing.observed            = true;
        r.listing.heads["main"]       = sha(3);
        r.listing.tags["v0.10.0"]     = sha(2);
    }
    HostWatch w;
    w.host = upm.code;
    for (const auto& d : deps) w.results.push_back(make_result(d, observe_dependency(d, src, src, "")));
    w.diff = diff(WatchState{}, w.host, w.results, kT0);
    return w;
}

std::set<std::string> keys_of(const jsonm::JsonValue& v) {
    std::set<std::string> out;
    if (v.is_object() && v.o) {
        for (const auto& kv : *v.o) out.insert(kv.first);
    }
    return out;
}

const jsonm::JsonValue& first(const jsonm::JsonValue& doc, const char* key) {
    static const jsonm::JsonValue kNull;
    const auto* arr = doc.find(key);
    return (arr && arr->is_array() && arr->a && !arr->a->empty()) ? arr->a->front() : kNull;
}

} // namespace

TEST(PackageWatchUpm, RepositoryFromUrl) {
    EXPECT_EQ(upm_repository("https://github.com/LUDIARS/Tela.git?path=/unity/com.ludiars.tela"),
              std::string("https://github.com/LUDIARS/Tela.git"));
    EXPECT_EQ(upm_repository("git+https://github.com/LUDIARS/Pictor.git?path=/unity/x"),
              std::string("https://github.com/LUDIARS/Pictor.git"));
    EXPECT_EQ(upm_repository("https://github.com/LUDIARS/Ergo.git"), std::string("https://github.com/LUDIARS/Ergo.git"));
    EXPECT_EQ(repository_name(upm_repository("git+https://github.com/LUDIARS/Pictor.git?path=/a")),
              std::string("Pictor"));
}

TEST(PackageWatchUpm, DependenciesFromManifest) {
    const auto upm  = fixture_upm();
    const auto ergo = fixture_ergo();
    EXPECT_EQ(upm.code, std::string("Example"));
    std::vector<std::string> skipped;
    const auto deps = upm_dependencies(upm, &ergo, &skipped);

    // git 依存のうち #<40 桁 SHA> の 2 つ。 #main は pin が無いので見ない。 file: とレジストリは入らない。
    ASSERT_EQ(deps.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(deps[0].name, std::string("com.ludiars.ergo.packages"));
    EXPECT_EQ(deps[0].code, std::string("Eg"));
    EXPECT_EQ(deps[0].owner, std::string("upm"));
    EXPECT_EQ(deps[0].repository, std::string("https://github.com/LUDIARS/Ergo.git"));
    EXPECT_EQ(deps[0].rev, std::string("b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    EXPECT_EQ(deps[0].track_branch, std::string("main"));
    EXPECT_TRUE(deps[0].expect_tag.empty());
    EXPECT_EQ(deps[1].name, std::string("com.ludiars.pictor"));
    EXPECT_EQ(deps[1].code, std::string("Pc"));  // Ergo のマニフェストで同じリポを指すパッケージのコード
    EXPECT_EQ(deps[1].repository, std::string("https://github.com/LUDIARS/Pictor.git"));

    ASSERT_EQ(skipped.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(skipped[0].find("com.ludiars.tela") != std::string::npos);
    EXPECT_TRUE(skipped[0].find("branch") != std::string::npos);
}

TEST(PackageWatchUpm, CodeFallsBackToRepositoryName) {
    const auto upm  = fixture_upm();
    const auto deps = upm_dependencies(upm, nullptr, nullptr);
    const WatchDependency* pictor = find_dep(deps, "com.ludiars.pictor");
    ASSERT_TRUE(pictor != nullptr);
    EXPECT_EQ(pictor->code, std::string("Pictor"));
}

TEST(PackageWatchUpm, WatchEmitsEventsForUpmHost) {
    const HostWatch w = watch_upm_fixture();
    EXPECT_EQ(w.host, std::string("Example"));
    ASSERT_EQ(w.diff.events.size(), static_cast<std::size_t>(2));  // tag + track (Pictor は取れない)
    EXPECT_EQ(w.diff.events[0].id, std::string("Example/com.ludiars.ergo.packages/tag/v0.10.0"));
    EXPECT_EQ(w.diff.events[0].update.behind.value_or(-1), 1);
    EXPECT_EQ(w.diff.events[0].update.to_rev, sha(2));
    EXPECT_EQ(w.diff.events[1].update.kind, UpdateKind::Track);
    EXPECT_EQ(w.diff.events[1].update.behind.value_or(-1), 2);
    // 次の操作は Unity の窓 (ergo_pkg update は UPM の pin を書かない)。
    EXPECT_TRUE(w.diff.events[0].update.next.find("Ergo Packages") != std::string::npos);
    EXPECT_TRUE(w.diff.events[0].update.next.find("#" + sha(2)) != std::string::npos);
    EXPECT_TRUE(w.diff.events[0].update.next.find("ergo_pkg") == std::string::npos);

    // 取れなかった依存は observed:false で、 「更新なし」ではない。
    ASSERT_EQ(w.results.size(), static_cast<std::size_t>(2));
    EXPECT_FALSE(w.results[1].observation.observed);
    EXPECT_FALSE(w.results[1].observation.reason.empty());
}

TEST(PackageWatchUpm, UpmReportPassesLeakCheckAndHidesRepository) {
    const HostWatch w = watch_upm_fixture();
    const auto out    = prepare_watch_outputs({w}, kT0);
    ASSERT_TRUE(out.ok);
    ASSERT_EQ(out.reports.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(out.reports[0].content.find("github.com") == std::string::npos);
    EXPECT_TRUE(out.reports[0].content.find("\"owner\":\"upm\"") != std::string::npos);
}

// Unity 側の WatchReportReader が読む見本 (fixtures/watch-report.json、 Unity の Fixtures とバイト一致) は、
// 書き手の出力と同じキーを持つ。 書き手が鍵を足したら見本も直す (Unity の読み手と同じ PR で)。
TEST(PackageWatchUpm, ReportFixtureHasWriterKeys) {
    const HostWatch w        = watch_upm_fixture();
    const jsonm::JsonValue a = host_report_json(w, kT0);
    const jsonm::JsonValue b = fixture("watch-report.json");
    ASSERT_TRUE(b.is_object());

    EXPECT_TRUE(keys_of(a) == keys_of(b));
    const auto& da = first(a, "dependencies");
    const auto& db = first(b, "dependencies");
    EXPECT_TRUE(keys_of(da) == keys_of(db));
    ASSERT_TRUE(da.find("pin") && db.find("pin") && da.find("track") && db.find("track"));
    EXPECT_TRUE(keys_of(*da.find("pin")) == keys_of(*db.find("pin")));
    EXPECT_TRUE(keys_of(*da.find("track")) == keys_of(*db.find("track")));
    EXPECT_TRUE(keys_of(first(da, "updates")) == keys_of(first(db, "updates")));
    EXPECT_TRUE(keys_of(first(a, "events")) == keys_of(first(b, "events")));
    EXPECT_TRUE(keys_of(first(a, "pending")) == keys_of(first(b, "pending")));
    EXPECT_FALSE(keys_of(first(b, "events")).empty());
    ASSERT_TRUE(a.find("sources") && b.find("sources"));
    EXPECT_TRUE(keys_of(*a.find("sources")) == keys_of(*b.find("sources")));
    EXPECT_FALSE(keys_of(*b.find("sources")).empty());
}
