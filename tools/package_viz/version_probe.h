#pragma once

// ergo_package_viz CLI: パッケージの版を外部コマンドで調べて札に入れる。
//   1. `revisor version show --repo <dir>`  (出所 "revisor")
//   2. `git -C <dir> describe --tags --long` (出所 "git")
//   3. どちらも取れなければ resolved.json の project(VERSION) のまま (出所 "cmake")
// 調べるのは checkout の場所が分かるもの (link の source_dir、 tool の path) だけ。
// コマンドが無い・失敗したときは黙って次へ進む (図は版不明でも書く)。

#include "ergo/package_model/package_model.h"

namespace ergo::package_viz::cli {

void probe_versions(package_model::PackageModel& model);

} // namespace ergo::package_viz::cli
