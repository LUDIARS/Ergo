#pragma once

// ergo_pkg watch / outdated の置き場所 (spec/feature/package-manager.md §0 / §2.3)。 どれもリポの外。
//
//   状態        --state > 利用者ディレクトリ (%LOCALAPPDATA%/ergo/pkg-watch、 $XDG_STATE_HOME/ergo/pkg-watch、
//               ~/.local/state/ergo/pkg-watch)
//   ミラー      --cache > ERGO_PKG_CACHE_DIR > ERGO_DEPENDENCY_CACHE_DIR の <cache>/mirrors/<Repo>.git
//   兄弟        --root > ERGO_PACKAGES_SIBLING_ROOT

#include "cli_args.h"

#include <string>

namespace ergo::package_cli {

/// 見つからなければ ""。
std::string watch_state_dir(const Args& args);
std::string watch_cache_dir(const Args& args);
std::string watch_sibling_root(const Args& args);

/// <state>/<code>.watch-state.json
std::string state_file(const std::string& state_dir, const std::string& code);

/// <host>/.ergo/watch-report.json のホストからの相対。
extern const char* const kWatchReportPath;
/// <out>/events.json の名前。
extern const char* const kEventsFile;

} // namespace ergo::package_cli
