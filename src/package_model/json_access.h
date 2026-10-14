#pragma once

// ergo_package_model の内部部品: jsonm の値から型付きで取り出す小さな関数と、
// ファイルの読み込み。 どれも読むだけで、 欠けた値は既定値を返す。

#include "ergo/common/json_min.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ergo::package_model::detail {

namespace jsonm = ergo::common::jsonm;

std::string              get_string(const jsonm::JsonValue& obj, const std::string& key);
bool                     get_bool(const jsonm::JsonValue& obj, const std::string& key);
std::vector<std::string> get_strings(const jsonm::JsonValue& obj, const std::string& key);
const jsonm::JsonValue*  get_object(const jsonm::JsonValue& obj, const std::string& key);
const jsonm::JsonArray*  get_array(const jsonm::JsonValue& obj, const std::string& key);

/// パスを UTF-8 の '/' 区切り文字列にする (C++17 / C++20 の u8string の型差を吸収)。
std::string generic_utf8(const std::filesystem::path& path);

/// ファイル全体を読む。 開けなければ false。
bool read_text_file(const std::string& path, std::string& out);

/// ファイルを読んで解析する。 失敗の理由を `error` に入れる。
bool parse_json_file(const std::string& path, jsonm::JsonValue& out, std::string* error);

} // namespace ergo::package_model::detail
