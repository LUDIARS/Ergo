#pragma once

// ergo_pkg status の入力集め (ファイルの時刻とテスト結果のファイル)。 解析は ergo_package_manager の
// test_results、 ここはどのファイルを読むかと時刻の取り方だけを持つ。

#include "ergo/package_manager/test_results.h"

#include <string>

namespace ergo::package_cli {

/// ファイルの更新時刻を `YYYY-MM-DDTHH:MM:SSZ` (UTC) で。 無ければ ""。
std::string file_mtime_utc(const std::string& path);

/// テスト結果: `junit` (明示) を優先し、 無ければ `<build>/Testing/Temporary/LastTest.log`、
/// どちらも無い・数えられなければ source = "unknown"。 明示した `junit` が読めない・数えられないときは
/// false と理由 (引数の誤り)。
bool collect_test_tally(const std::string& build, const std::string& junit,
                        package_manager::TestTally& out, std::string* error);

} // namespace ergo::package_cli
