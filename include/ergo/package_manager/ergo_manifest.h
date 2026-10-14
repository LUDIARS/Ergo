#pragma once

// Ergo のマニフェスト (リポ直下 ergo-packages.json、 schema 1) のうち、 ホストの宣言を検査するのに
// 要る部分 (パッケージの名前・種類・取得元・pin と、 modules / observe の名前) だけを読む。
// `host` 節を持つファイルはホストの宣言なので、 Ergo のマニフェストとしては読まない (エラー)。

#include "ergo/common/json_min.h"

#include <map>
#include <string>
#include <vector>

namespace ergo::package_manager {

namespace jsonm = ergo::common::jsonm;

/// packages.<name> (Ergo のマニフェスト、 またはホスト宣言の非公開 packages)。
struct ManifestPackage {
    std::string name;
    std::string kind;        ///< "link" / "tool"
    std::string code;
    std::string repository;
    std::string rev;
    std::string expect_tag;
};

struct ErgoManifest {
    int                          schema = 0;
    std::vector<ManifestPackage> packages;  ///< 名前順
    std::vector<std::string>     modules;   ///< 名前順
    std::vector<std::string>     observe;   ///< 名前順
    /// modules.<m>.needs (モジュール名 → 要るパッケージ)。 needs の無いモジュールは載らない。
    std::map<std::string, std::vector<std::string>> module_needs;

    const ManifestPackage* find(const std::string& name) const;
    /// packages / modules / observe のどれかにある名前か (PKG-15 の照合用)。
    bool has_name(const std::string& name) const;
    /// name を needs に持つモジュール (名前順)。
    std::vector<std::string> modules_needing(const std::string& name) const;
};

/// 失敗 (オブジェクトでない、 `host` 節がある、 schema が 1 でない) なら false と理由。
bool read_ergo_manifest(const jsonm::JsonValue& doc, ErgoManifest& out, std::string* error);

} // namespace ergo::package_manager
