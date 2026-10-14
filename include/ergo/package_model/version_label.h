#pragma once

/// ergo_package_model — 図の札に載せる版の文字列を作る (純粋関数)。
///
/// 版の情報源は呼び出し側が取る (CLI は `revisor version show --repo <dir>`、
/// 使えなければ `git describe --tags --long`)。 ここでは出力の文字列を札の形に直すだけ。

#include "ergo/package_model/package_model.h"

#include <string>

namespace ergo::package_model {

/// `git describe --tags --long` の出力を札にする。
///   "v2.2.0-12-g625d83d"   → "2.2.0+12"
///   "v2.2.0-0-g625d83d"    → "2.2.0"
///   "release-3-4-gabc1234" → "release-3+4" (タグ名はそのまま、 先頭の v だけ落とす)
/// タグの無い出力 (SHA だけ) や空は "" を返す。
std::string version_from_describe(const std::string& describe);

/// `revisor version show` の出力から最初の MAJOR.MINOR.PATCH を取り出す ("" = 無し)。
std::string version_from_revisor(const std::string& output);

/// 札の 1 行: "版 2.2.0+12 [git]"。 版が無ければ "版 不明"。
std::string version_line(const VersionLabel& version);

/// SHA の先頭 7 桁 (短ければそのまま)。
std::string short_sha(const std::string& sha);

} // namespace ergo::package_model
