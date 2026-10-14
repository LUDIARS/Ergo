// ergo_package_model: P4 の入力 (resolved.json の host.id、 watch-report.json、 status.json) の読み手。
// 見本の読み取り・欠けた入力・壊れた入力・schema 違い。

#include "gtest/gtest.h"

#include "ergo/package_model/host_state_reader.h"
#include "ergo/package_model/resolved_reader.h"

#include <string>

using namespace ergo::package_model;

namespace {

const std::string kDir = std::string(ERGO_PACKAGE_MODEL_FIXTURES) + "/host_state";

HostStatePaths fixture_paths() {
    return {kDir + "/ergo-packages.resolved.json", kDir + "/watch-report.json",
            kDir + "/ergo-packages.status.json"};
}

} // namespace

TEST(PackageModelHostState, ReadsResolvedHostId) {
    PackageModel m;
    ASSERT_TRUE(read_resolved_file(kDir + "/ergo-packages.resolved.json", m));
    EXPECT_EQ(m.host_id, std::string("Example"));
    EXPECT_EQ(m.mode, std::string("M1"));

    PackageModel none;
    ASSERT_TRUE(read_resolved_text(R"({"schema":1,"packages":{}})", none));
    EXPECT_EQ(none.host_id, std::string());
}

TEST(PackageModelHostState, LoadsAllThreeInputs) {
    const auto s = load_host_state(fixture_paths());
    ASSERT_TRUE(s.has_resolved);
    ASSERT_TRUE(s.has_report);
    ASSERT_TRUE(s.has_status);
    EXPECT_TRUE(s.notes.empty());

    EXPECT_EQ(s.report.host, std::string("Example"));
    EXPECT_EQ(s.report.revisor, std::string("ok"));
    ASSERT_EQ(s.report.dependencies.size(), 3u);
    const auto* ergo = s.report.find("ergo");
    ASSERT_TRUE(ergo != nullptr);
    EXPECT_EQ(ergo->pin_tag, std::string("v0.9.0"));
    ASSERT_TRUE(ergo->track_behind.has_value());
    EXPECT_EQ(*ergo->track_behind, 18);
    const auto* priv = s.report.find("examplepriv");
    ASSERT_TRUE(priv != nullptr);
    EXPECT_TRUE(priv->is_private);
    EXPECT_FALSE(priv->observed);
    EXPECT_FALSE(priv->track_behind.has_value());
    EXPECT_FALSE(priv->on_track.has_value());

    const auto* pictor = s.report.find("pictor");
    ASSERT_TRUE(pictor != nullptr);
    ASSERT_EQ(pictor->updates.size(), 9u);
    const auto& review = pictor->updates[5];
    EXPECT_EQ(review.kind, std::string("review"));
    EXPECT_EQ(review.origin, std::string("pf"));
    ASSERT_TRUE(review.review.has_value());
    EXPECT_EQ(*review.review, 128);
    EXPECT_EQ(review.head, std::string("4f0e9a1"));
    EXPECT_FALSE(pictor->updates[8].review.has_value());   // run は番号を持たない

    EXPECT_EQ(s.status.host, std::string("Example"));
    EXPECT_EQ(s.status.configured_at, std::string("2026-10-15T01:00:00Z"));
    EXPECT_EQ(s.status.tests.source, std::string("junit"));
    EXPECT_EQ(s.status.tests.total, 5);
    EXPECT_EQ(s.status.tests.passed, 3);
    EXPECT_EQ(s.status.tests.failed, 1);
    EXPECT_EQ(s.status.tests.skipped, 1);
}

TEST(PackageModelHostState, MissingInputsAreNotErrors) {
    HostStatePaths p;
    p.resolved = kDir + "/does-not-exist.json";
    const auto s = load_host_state(p);
    EXPECT_FALSE(s.has_resolved);
    EXPECT_FALSE(s.has_report);
    EXPECT_FALSE(s.has_status);
    EXPECT_EQ(s.notes.size(), 3u);
}

TEST(PackageModelHostState, RejectsMalformedAndOtherSchemas) {
    WatchReport r;
    std::string error;
    EXPECT_FALSE(read_watch_report_text("{", r, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(read_watch_report_text(R"({"schema":2,"host":"Example"})", r, &error));

    HostStatus st;
    EXPECT_FALSE(read_host_status_text("[]", st, &error));
    EXPECT_FALSE(read_host_status_text(R"({"schema":3})", st, &error));
}

TEST(PackageModelHostState, UnknownTestsMeanNotRun) {
    HostStatus st;
    ASSERT_TRUE(read_host_status_text(
        R"({"configured_at":"","head":"","host":"Example","schema":1,"tests":{"source":"unknown"}})", st));
    EXPECT_EQ(st.tests.source, std::string("unknown"));
    EXPECT_EQ(st.tests.total, 0);
    // 知らない source も未実行として扱う (数を信じない)。
    ASSERT_TRUE(read_host_status_text(R"({"schema":1,"tests":{"source":"ci","total":9}})", st));
    EXPECT_EQ(st.tests.source, std::string("unknown"));
    EXPECT_EQ(st.tests.total, 0);
}
