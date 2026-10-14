#pragma once

// ergo_package_scan の内部部品: 1 つの読み取り元 (観測対象のチェックアウト / Ergo / Ergo の Web ツール)
// と、 そこから読んだ package.json・.gitmodules・gitlink。 読むのは Scope の範囲内だけ。

#include "ergo/package_scan/gitmodules_reader.h"
#include "ergo/package_scan/package_json_reader.h"
#include "scope.h"

#include <map>
#include <string>
#include <vector>

namespace ergo::package_scan::detail {

struct Source {
    std::string node_id;
    Scope       scope;
    std::string package_dir;   ///< package.json のあるディレクトリ (scope の根からの相対、 "" = 根)
    bool        read_git = false;   ///< .gitmodules と .git/index も読む

    bool                               has_package = false;
    PackageJson                        package;
    std::string                        package_file;    ///< 表示名
    std::vector<Submodule>             submodules;
    std::string                        gitmodules_file; ///< 表示名
    std::map<std::string, std::string> gitlinks;        ///< submodule のパス → SHA
    bool                               gitlinks_known = false;
};

/// 読めたものだけ埋める。 読めなかった理由は `notes` に足す。
void read_source(Source& source, std::vector<std::string>& notes);

} // namespace ergo::package_scan::detail
