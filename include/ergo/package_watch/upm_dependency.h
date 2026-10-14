#pragma once

// Unity プロジェクト (UPM ホスト) の依存を、 更新を確かめる依存 (WatchDependency) にする
// (spec/feature/package-manager.md §6.3: `ergo-packages.json` が無く `Packages/manifest.json` がある
// ディレクトリ。 host.code は `.ergo/host.json`)。 pin の持ち主はその Unity プロジェクトで、 owner は "upm"。
// 宣言 (ergo_package_manager の UPM の読み手) から列挙するだけで、 ネットワークにもファイルにも触らない。

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/upm_declaration.h"
#include "ergo/package_watch/watch_dependency.h"

#include <string>
#include <vector>

namespace ergo::package_watch {

/// UPM ホストの依存の持ち主 (manifest.json の `#<sha>` は Unity の窓が書き換える)。
extern const char* const kUpmOwner;

/// UPM の git URL から取得元を取り出す: 先頭の "git+" と "?path=..." を落とす
/// ("git+https://host/LUDIARS/Tela.git?path=/unity/x" → "https://host/LUDIARS/Tela.git")。
std::string upm_repository(const std::string& url);

/// UPM ホストの git 依存を列挙する。 コードは Ergo のマニフェストで同じリポを指すパッケージのコード
/// (無ければリポの末尾名)。 `#main` などブランチ指定・ `#` 無しは pin が無いので skipped に理由を積む。
/// レジストリ・`file:` の依存は read_upm_dependencies の時点で入らない。 並びは名前順。
std::vector<WatchDependency> upm_dependencies(const pm::UpmDeclaration& upm,
                                              const pm::ErgoManifest* ergo,
                                              std::vector<std::string>* skipped);

} // namespace ergo::package_watch
