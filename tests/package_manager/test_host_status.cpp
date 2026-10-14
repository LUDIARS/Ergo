// ergo_package_manager: ergo_pkg status の集計と書き手 (spec/feature/package-manager.md §5.1)。
// JUnit / LastTest.log の数え方、 status.json のバイト列 (P4 の見本と一致)、 漏れ検査、 決定性。

#include "gtest/gtest.h"

#include "ergo/package_manager/host_status.h"
#include "ergo/package_manager/leak_check.h"
#include "ergo/package_manager/test_results.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

using namespace ergo::package_manager;

namespace {

const std::string kStatusFixtures = std::string(ERGO_PACKAGE_MANAGER_FIXTURES) + "/status";

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

/// P4 の見本 (tests/package_model/fixtures/host_state/ergo-packages.status.json) を作った入力。
HostStatusInput fixture_input() {
    HostStatusInput in;
    in.host          = "Example";
    in.head          = "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d";
    in.configured_at = "2026-10-15T01:00:00Z";
    parse_junit(slurp(kStatusFixtures + "/ctest-junit.xml"), in.tests);   // 数え方は CountsJunit が確かめる
    return in;
}

} // namespace

TEST(PackageManagerHostStatus, CountsJunit) {
    TestTally t;
    ASSERT_TRUE(parse_junit(slurp(kStatusFixtures + "/ctest-junit.xml"), t));
    EXPECT_EQ(t.source, std::string("junit"));
    EXPECT_EQ(t.total, 5);
    EXPECT_EQ(t.passed, 3);
    EXPECT_EQ(t.failed, 1);
    EXPECT_EQ(t.skipped, 1);
}

TEST(PackageManagerHostStatus, JunitWithoutTestcasesIsUnknown) {
    TestTally t;
    EXPECT_FALSE(parse_junit("<testsuites><testsuite tests=\"0\"/></testsuites>", t));
    EXPECT_EQ(t.source, std::string("unknown"));
    // <testcases> は <testcase> ではない。 error 子も失敗に数える。
    ASSERT_TRUE(parse_junit("<testcases/><testcase name=\"a\"><error/></testcase>", t));
    EXPECT_EQ(t.total, 1);
    EXPECT_EQ(t.failed, 1);
}

TEST(PackageManagerHostStatus, CountsLastTestLog) {
    TestTally t;
    ASSERT_TRUE(parse_last_test_log(slurp(kStatusFixtures + "/LastTest.log"), t));
    EXPECT_EQ(t.source, std::string("last-test-log"));
    EXPECT_EQ(t.total, 3);
    EXPECT_EQ(t.passed, 2);
    EXPECT_EQ(t.failed, 1);
    EXPECT_FALSE(parse_last_test_log("Start testing\nEnd testing\n", t));
}

TEST(PackageManagerHostStatus, WritesTheP4FixtureBytes) {
    std::string expected =
        slurp(std::string(ERGO_PACKAGE_MANAGER_SOURCE) +
              "/tests/package_model/fixtures/host_state/ergo-packages.status.json");
    ASSERT_FALSE(expected.empty());
    // core.autocrlf の checkout では見本の改行が CRLF になるので LF に戻して比べる。
    expected.erase(std::remove(expected.begin(), expected.end(), '\r'), expected.end());
    EXPECT_EQ(host_status_text(fixture_input()), expected);
    EXPECT_EQ(host_status_text(fixture_input()), host_status_text(fixture_input()));
}

TEST(PackageManagerHostStatus, UnknownTestsCarryNoCounts) {
    HostStatusInput in;
    in.host = "Example";
    EXPECT_EQ(host_status_text(in),
              std::string("{\"configured_at\":\"\",\"head\":\"\",\"host\":\"Example\",\"schema\":1,"
                          "\"tests\":{\"source\":\"unknown\"}}\n"));
}

TEST(PackageManagerHostStatus, StatusHasNoLeaks) {
    EXPECT_TRUE(find_leaks(host_status_to_json(fixture_input())).empty());
}
