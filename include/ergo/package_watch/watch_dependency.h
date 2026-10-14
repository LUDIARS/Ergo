#pragma once

// 更新を確かめる依存 1 つ (spec/feature/package-manager.md §3: ホスト H がパッケージ P を pin S で使う、
// または Ergo のマニフェストが P を S で持つ)。 宣言 (ergo_package_manager の読み手) から列挙するだけで、
// ネットワークにもファイルにも触らない。

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/host_declaration.h"

#include <string>
#include <vector>

namespace ergo::package_watch {

namespace pm = ergo::package_manager;

/// track.branch が無いときの追跡ブランチ。
extern const char* const kDefaultTrackBranch;
/// Ergo 自身を指す依存の名前 (host.ergo)。
extern const char* const kErgoDependency;
/// 宣言を持たない Ergo のチェックアウト自身を見るときのホストのコード。
extern const char* const kErgoHostCode;

struct WatchDependency {
    std::string name;
    std::string code;
    std::string owner;         ///< pin の持ち主: "host" (Ergo 自身・非公開) / "ergo" (共有)
    std::string repository;    ///< 取得元 (報告には載せない)
    std::string rev;           ///< pin S (40 桁 SHA)
    std::string expect_tag;
    std::string track_branch;  ///< 空にならない (既定 kDefaultTrackBranch)
    bool        is_private = false;
};

/// "https://host/LUDIARS/Pictor.git" → "Pictor" (兄弟チェックアウト・ミラーの名前)。 取れなければ ""。
std::string repository_name(const std::string& repository);

/// ホストの宣言から: host.ergo、 host.use のうち Ergo のマニフェストの link パッケージ、 非公開 link パッケージ。
/// 取得元か 40 桁 SHA の pin が無いものは見られないので skipped に理由を積む。 並びは名前順。
std::vector<WatchDependency> host_dependencies(const pm::HostDeclaration& host,
                                               const pm::ErgoManifest* ergo,
                                               std::vector<std::string>* skipped);

/// Ergo のチェックアウト自身をホストとして見る: マニフェストの link パッケージ全て。
std::vector<WatchDependency> ergo_dependencies(const pm::ErgoManifest& ergo,
                                               std::vector<std::string>* skipped);

} // namespace ergo::package_watch
