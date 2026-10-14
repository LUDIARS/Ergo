#pragma once

// ergo_pkg のファイル読み書き (バイト単位。 改行は変換しない)。

#include "ergo/common/json_min.h"

#include <optional>
#include <string>

namespace ergo::package_cli {

namespace jsonm = ergo::common::jsonm;

/// 無ければ nullopt。
std::optional<std::string> read_file(const std::string& path);

/// 親ディレクトリを作って書く。 失敗したら false。
bool write_file(const std::string& path, const std::string& content);

/// 読んで解析する。 無い・壊れているなら false と理由。
bool read_json(const std::string& path, jsonm::JsonValue& out, std::string* error);

/// dir と相対パスを '/' でつなぐ。
std::string join(const std::string& dir, const std::string& rel);

} // namespace ergo::package_cli
