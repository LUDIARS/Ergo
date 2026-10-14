#pragma once

// ergo_pkg watch / outdated の共通部品: ホストの依存を全て観測して判定する (observe → detect)。

#include "watch_host.h"

#include "ergo/package_watch/ref_source.h"
#include "ergo/package_watch/watch_diff.h"
#include "ergo/package_watch/watch_state.h"

#include <vector>

namespace ergo::package_cli {

/// previous の指紋を使う (同じなら同期しない)。 outdated は空の状態を渡す。
std::vector<pw::DependencyResult> observe_host(const WatchHost& host, pw::IRefSource& refs,
                                               pw::IHistorySource& history, const pw::WatchState& previous);

} // namespace ergo::package_cli
