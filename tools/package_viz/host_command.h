#pragma once

// ergo_package_viz CLI の P4 ホスト状態図 (`--view host`) 部分。 入力の場所を決めて
// HostState を読む所までを持つ (図の書き出しは main.cpp)。

#include "cli_options.h"

#include "ergo/package_model/host_state_reader.h"

namespace ergo::package_viz::cli {

/// 入力の場所: resolved は `--resolved` か `<build>/ergo-packages.resolved.json`、 報告は `--report` か
/// `<host>/.ergo/watch-report.json` (`--host` が無ければ読まない)、 状態は `--status` か
/// `<build>/ergo-packages.status.json`。
package_model::HostStatePaths host_paths_from_options(const Options& options);

} // namespace ergo::package_viz::cli
