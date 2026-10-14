#pragma once

// ホストの宣言 (ホストのリポ直下 ergo-packages.json の `host` 節、 spec/feature/package-manager.md §2.1、
// 形式は spec/data/ergo-packages.md「ホストの宣言」) を読む。 読むだけで、 意味の検査は
// declaration_check.h が行う。 形の不備 (型違い) は MANIFEST の error として返す。

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/finding.h"

#include <map>
#include <string>
#include <vector>

namespace ergo::package_manager {

/// host.ergo — ホストが乗る Ergo 自身の pin。
struct ErgoPin {
    bool        present = false;
    std::string repository;
    std::string rev;
    std::string expect_tag;
    std::string track_branch;  ///< host.ergo.track.branch (無ければ "" = 既定の main)
};

struct HostDeclaration {
    int                                schema = 0;
    std::string                        code;         ///< プロジェクトコード (resolved.json の host.id)
    ErgoPin                            ergo;
    std::vector<std::string>           use;          ///< ERGO_PACKAGES の既定値 (書いた順)
    std::vector<std::string>           optional;     ///< ソースが見つからなければ使わない
    std::map<std::string, std::string> features;     ///< 名前 → ERGO_PACKAGE_FEATURES_<NAME> の既定値
    std::map<std::string, std::string> legacy_vars;  ///< 旧変数名 → "ergo.source_dir" / "<pkg>.source_dir"
    std::vector<ManifestPackage>       packages;     ///< 非公開パッケージ (名前順)
};

/// トップレベルに `host` 節を持つオブジェクトか。
bool is_host_declaration(const jsonm::JsonValue& doc);

/// ホストの宣言を読む。 ホストの宣言でなければ false (findings に MANIFEST の error)。
/// 形の不備は findings に積み、 読めた部分は out に入れて true を返す。
bool read_host_declaration(const jsonm::JsonValue& doc, HostDeclaration& out,
                           std::vector<Finding>& findings);

} // namespace ergo::package_manager
