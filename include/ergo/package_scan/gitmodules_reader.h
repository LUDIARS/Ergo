#pragma once

/// ergo_package_scan — `.gitmodules` の読み取り (git の設定形式のうち submodule 節だけ)。
///
/// `[submodule "<名>"]` の節から `path` と `url` を行番号付きで取り出す。 コメント (`#` / `;`)
/// と他の節は読み飛ばす。 `path` の無い節は返さない。

#include <string>
#include <vector>

namespace ergo::package_scan {

struct Submodule {
    std::string name;
    std::string path;
    std::string url;
    int         path_line = 0;
    int         url_line  = 0;
};

std::vector<Submodule> parse_gitmodules(const std::string& text);

/// url の末尾のリポ名 (".git" と末尾の '/' を除く)。 例: ".../Lapilli.git" → "Lapilli"。
std::string repo_name_from_url(const std::string& url);

} // namespace ergo::package_scan
