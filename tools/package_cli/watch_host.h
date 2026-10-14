#pragma once

// ergo_pkg watch / outdated が見るホスト 1 つ: コード (状態ファイルと出来事の鍵) と依存の一覧。
//
//   * ホストの宣言 (`host` 節) があればそれ (host.ergo・use の共有パッケージ・非公開パッケージ)
//   * 無く、 ergo-packages.json が Ergo のマニフェストなら Ergo のチェックアウト自身 (コード "Ergo")
//   * ergo-packages.json が無く Packages/manifest.json があれば Unity プロジェクト (UPM ホスト、 W7)。
//     コードは .ergo/host.json、 依存は manifest.json の LUDIARS の git 依存 (owner "upm")
//   * どれでもなければ誤り

#include "cli_args.h"

#include "ergo/package_watch/watch_dependency.h"

#include <string>
#include <vector>

namespace ergo::package_cli {

namespace pw = ergo::package_watch;

struct WatchHost {
    std::string                      dir;
    std::string                      code;
    std::vector<pw::WatchDependency> dependencies;  ///< 名前順
    std::vector<std::string>         notes;         ///< 見られない依存の理由など
};

/// 失敗 (宣言もマニフェストも無い・壊れている・コードが決まらない) なら false と理由。
bool load_watch_host(const Args& args, const std::string& dir, WatchHost& out, std::string* error);

} // namespace ergo::package_cli
