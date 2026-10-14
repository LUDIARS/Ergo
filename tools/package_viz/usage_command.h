#pragma once

// ergo_package_viz CLI の観測 (P3 usage と --scan) 部分。 兄弟チェックアウトを読んで
// UsageModel を作る所までを持つ (図の書き出しは main.cpp)。

#include "cli_options.h"

#include "ergo/package_scan/usage_model.h"

#include <string>

namespace ergo::package_viz::cli {

/// マニフェストを決めて (`--manifest`、 無ければ resolved.json の記録を Ergo のソースから解く)
/// 観測対象を読む。 失敗したら false と理由 (使い方・入力の誤り)。
bool scan_from_options(const Options& options, package_scan::UsageModel& usage, std::string* error);

/// `--scan`: 辺を根拠付きで 1 行ずつ、 未観測と読んだファイルを表示する。 終了コード 0。
int print_scan(const package_scan::UsageModel& usage);

} // namespace ergo::package_viz::cli
