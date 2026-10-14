#pragma once

// 書式を保つ JSON の書き換え (spec/feature/package-manager.md §2.2: キー順・改行・字下げを変えず、 変えるのは
// 該当の値だけ)。 jsonm で読み直して書き出すと書式が崩れるので、 テキストの範囲を直接差し替える。
//
// 経路 (path) はオブジェクトのキーの並び。 配列の中へは入らない (宣言の書き換えには要らない)。
// 足すメンバ・要素は兄弟と同じ区切り (改行と字下げ、 または 1 行のまま) を写す。 空のコンテナに足すときは
// 単純な値なら 1 行 (`["a"]` / `{ "a": "b" }`)、 オブジェクトの値なら字下げした複数行にする。
// 改行は文書が CRLF なら CRLF。 どの関数も失敗したら text を変えずに false を返す。

#include <string>
#include <utility>
#include <vector>

namespace ergo::package_manager {

using JsonPath = std::vector<std::string>;

/// JSON の文字列リテラル (引用符つき、 必要なものだけエスケープ)。
std::string json_quote(const std::string& value);

/// path の値があるか。
bool json_has(const std::string& text, const JsonPath& path);

/// path の値 (既存) を literal に差し替える。
bool json_replace(std::string& text, const JsonPath& path, const std::string& literal);

/// object_path のオブジェクトに key を置く。 あれば値の差し替え、 無ければ末尾に足す。
bool json_set_member(std::string& text, const JsonPath& object_path, const std::string& key,
                     const std::string& literal);

/// object_path のオブジェクトに key: { fields... } を足す (key が既にあれば失敗)。 fields は (キー, リテラル)。
bool json_add_object_member(std::string& text, const JsonPath& object_path, const std::string& key,
                            const std::vector<std::pair<std::string, std::string>>& fields);

/// object_path のオブジェクトから key を外す (無ければ変えずに true)。
bool json_remove_member(std::string& text, const JsonPath& object_path, const std::string& key);

/// array_path の配列 (文字列の配列) の末尾に value を足す (既にあれば変えずに true)。
bool json_append_string(std::string& text, const JsonPath& array_path, const std::string& value);

/// array_path の配列から文字列 value を外す (無ければ変えずに true)。
bool json_remove_string(std::string& text, const JsonPath& array_path, const std::string& value);

} // namespace ergo::package_manager
