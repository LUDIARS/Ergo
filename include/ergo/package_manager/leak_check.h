#pragma once

// 漏れ検査 (spec/feature/package-manager.md §8)。 追跡されるファイル (ホストの宣言、 ブートストラップ、
// .ergo/host.json) に開発機の絶対パスや秘密を書いていないかを機械で確かめる。
//
// 拒む形:
//   * ドライブ文字で始まる値 ("C:/..." "D:\...")
//   * "/Users/" または "/home/" を含む値
//   * "\\" で始まる UNC パス
//   * キーの名前に token / secret / authorization を含む値 (大文字小文字を区別しない)

#include "ergo/common/json_min.h"
#include "ergo/package_manager/finding.h"

#include <string>
#include <vector>

namespace ergo::package_manager {

namespace jsonm = ergo::common::jsonm;

struct Leak {
    std::string where;   ///< JSON Pointer ("/host/legacy_vars/X") またはテキストの "line N"
    std::string reason;
};

bool looks_like_local_path(const std::string& value);
bool is_secret_key(const std::string& key);

/// JSON の全ての値とキーを調べる (出現順)。
std::vector<Leak> find_leaks(const jsonm::JsonValue& doc);

/// テキスト (ブートストラップなど) の各行を空白と引用符で区切った語のうち、 絶対パスに見えるもの。
std::vector<Leak> find_text_leaks(const std::string& text);

/// 1 件ごとに LEAK の error (subject はファイル名)。
std::vector<Finding> leak_findings(const std::vector<Leak>& leaks, const std::string& subject);

} // namespace ergo::package_manager
