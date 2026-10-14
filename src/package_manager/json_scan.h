#pragma once

// ergo_package_manager の内部部品: JSON テキストの範囲 (バイト位置) を読む走査器。 値を作り直さないので、
// json_edit.cpp はここで得た範囲だけを差し替えて書式を保つ。 入力は jsonm で解析できる JSON を前提にする。

#include "ergo/package_manager/json_edit.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ergo::package_manager::detail {

/// コンテナの 1 要素。 オブジェクトのメンバは key_begin がキーの '"'、 配列の要素は key_begin == value_begin。
struct JsonEntry {
    std::size_t key_begin   = 0;
    std::size_t value_begin = 0;
    std::size_t value_end   = 0;  ///< 値の直後
    std::string key;              ///< オブジェクトのメンバだけ (解いた文字列)
};

/// '{' / '[' から対応する閉じまで。
struct JsonContainer {
    bool                   object = false;
    std::size_t            open   = 0;
    std::size_t            close  = 0;  ///< '}' / ']' の位置
    std::vector<JsonEntry> entries;
};

std::size_t skip_ws(const std::string& t, std::size_t i);

/// t[begin] から始まる値の直後の位置。 壊れていれば false。
bool scan_value_end(const std::string& t, std::size_t begin, std::size_t& end);

/// t[open] の '{' / '[' を走査する。
bool scan_container(const std::string& t, std::size_t open, JsonContainer& out);

/// path の値の範囲 [begin, end)。 無ければ false。
bool locate_value(const std::string& t, const JsonPath& path, std::size_t& begin, std::size_t& end);

/// path のコンテナ (オブジェクト / 配列)。
bool locate_container(const std::string& t, const JsonPath& path, JsonContainer& out);

/// pos を含む行の先頭の空白 (字下げ)。
std::string line_indent(const std::string& t, std::size_t pos);

/// 文書の改行 ("\r\n" を含めば "\r\n")。
std::string newline_of(const std::string& t);

/// 字下げの 1 段 (最初に字下げされた行の空白。 無ければ 2 空白)。
std::string indent_unit(const std::string& t);

} // namespace ergo::package_manager::detail
