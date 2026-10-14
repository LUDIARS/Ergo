#pragma once

// Unity プロジェクト (UPM ホスト) の宣言を読む: `Packages/manifest.json` の LUDIARS の git 依存と
// `.ergo/host.json` の code (spec/feature/package-manager.md §6.3)。 list と watch が共有する。

#include "ergo/package_manager/upm_declaration.h"

#include <string>

namespace ergo::package_cli {

namespace pm = ergo::package_manager;

/// `<dir>/Packages/manifest.json`。
extern const char* const kUpmManifestPath;

/// Unity プロジェクトなら UPM の宣言を読む。 manifest.json が無い・壊れているなら false。
/// `.ergo/host.json` が無ければ code は ""。
bool load_upm_declaration(const std::string& dir, pm::UpmDeclaration& out);

} // namespace ergo::package_cli
