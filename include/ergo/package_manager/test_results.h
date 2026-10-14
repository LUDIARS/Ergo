#pragma once

// テスト結果の集計 (spec/feature/package-manager.md §5.1、 ergo_pkg status)。
// ctest の `--output-junit` の XML を優先し、 無ければ `Testing/Temporary/LastTest.log` を読む。
// どちらも本文を解析するだけで、 ファイルを開かない (開くのは CLI)。 テスト名・出力・パスは持たない。

#include <string>

namespace ergo::package_manager {

/// 数の要約。 source は "junit" / "last-test-log" / "unknown" (読めなかった = 未実行)。
struct TestTally {
    std::string source = "unknown";
    long long   total   = 0;
    long long   passed  = 0;
    long long   failed  = 0;   ///< failure と error の和
    long long   skipped = 0;   ///< skipped と disabled (ctest の Not Run / Disabled)
};

/// ctest の JUnit XML。 `<testcase>` を数え、 子の `<failure>` / `<error>` を失敗、
/// `<skipped>` か status="notrun" / "disabled" を未実行とする。 `<testcase>` が 1 つも無ければ false。
bool parse_junit(const std::string& xml, TestTally& out);

/// ctest の LastTest.log。 "Test Passed." / "Test Failed" / "Test Not Run" で始まる行を数える。
/// どれも無ければ false。
bool parse_last_test_log(const std::string& log, TestTally& out);

} // namespace ergo::package_manager
