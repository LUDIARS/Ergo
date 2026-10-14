#pragma once

// ergo_package_scan の内部部品: 依存先 (file: のパス・パッケージ名・submodule の url) を
// ノード ID に帰属させる。 パスの計算だけで、 依存先のファイルは読まない (範囲外を読まない)。
//
// 帰属の順 (file:):
//   1. 同じ読み取り元の submodule の中 → その submodule の帰属先
//   2. Ergo の tool パッケージ (lib/lapilli など) の中 → そのパッケージ
//   3. Ergo の Web ツール (ergo_paths) の中 → そのノード、 Ergo の他の場所 → "ergo"
//   4. 観測対象のチェックアウトの中 → その観測対象
//   5. 兄弟の根の直下 <Seg> の中 → 名前の一致する tool パッケージ、 無ければ "repo:<Seg>"
//   6. どれでもない → "npm:<依存名>"
// submodule の url はリポ名で 観測対象 (path_hint の末尾) → tool パッケージ → "repo:<名>"。
// file: 以外の依存は読んだ package.json の name の表 → "npm:<依存名>"。

#include "ergo/package_scan/manifest_reader.h"
#include "source.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ergo::package_scan::detail {

struct Resolver {
    std::filesystem::path                ergo_root;    ///< 正規化済み
    std::filesystem::path                root;         ///< 正規化済み (空 = 兄弟の根なし)
    const ScanManifest*                  manifest = nullptr;
    std::vector<std::string>             ergo_paths;   ///< 安全なものだけ
    std::map<std::string, std::string>   npm_names;    ///< package.json の name → ノード ID

    std::string for_submodule(const Submodule& submodule) const;
    std::string for_dependency(const Source& source, const PackageDep& dep) const;

private:
    std::string for_repo_name(const std::string& repo) const;
    std::string for_path(const Source& source, const std::filesystem::path& abs) const;
};

} // namespace ergo::package_scan::detail
