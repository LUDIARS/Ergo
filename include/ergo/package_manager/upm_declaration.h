#pragma once

// Unity プロジェクト (UPM ホスト) の宣言を読む: .ergo/host.json (`init --upm` が作る、 host.code だけ) と
// Packages/manifest.json の dependencies のうち LUDIARS のリポを指す git URL。
// 判定はしない (W3 以降)。 一覧 (`ergo_pkg list`) に出すためだけに読む。

#include "ergo/common/json_min.h"

#include <string>
#include <vector>

namespace ergo::package_manager {

namespace jsonm = ergo::common::jsonm;

struct UpmDependency {
    std::string name;        ///< UPM のパッケージ名
    std::string url;         ///< '#' より前
    std::string rev;         ///< '#' より後 (無ければ "")
    bool        branch_ref = false;  ///< rev が 40 桁 SHA でない (#main など)
};

struct UpmDeclaration {
    std::string                code;
    std::vector<UpmDependency> dependencies;  ///< 名前順
};

/// .ergo/host.json の code ("" なら無い)。
std::string read_upm_host_code(const jsonm::JsonValue& doc);

/// Packages/manifest.json から LUDIARS の git 依存だけを取り出す。
std::vector<UpmDependency> read_upm_dependencies(const jsonm::JsonValue& manifest);

} // namespace ergo::package_manager
