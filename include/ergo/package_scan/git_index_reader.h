#pragma once

/// ergo_package_scan — git の index (`.git/index`) から gitlink (mode 160000) の SHA を読む。
///
/// git を起動せずに読むための最小の解析で、 版 2 / 3 だけを扱う (版 4 の接頭辞圧縮は
/// 読まない → false)。 書き換えはしない。 読む場所の決定 (`.git` がチェックアウトの中の
/// ディレクトリのときだけ) は scan.cpp の責務。

#include <map>
#include <string>

namespace ergo::package_scan {

/// 成功すれば true。 `gitlinks` はパス ('/' 区切り) → 40 桁 SHA (小文字)。
bool parse_index_gitlinks(const std::string& bytes, std::map<std::string, std::string>& gitlinks,
                          std::string* error = nullptr);

} // namespace ergo::package_scan
