#pragma once

/// ergo_package_scan — JSON の各値が何行目から始まるかの索引 (根拠の行番号に使う)。
///
/// `ergo_common::jsonm` は位置を持たないので、 同じ文字列をもう一度だけ走査して
/// 「キーの道筋 → 行」を作る。 値そのものは解釈しない (読むのは jsonm)。
/// 道筋はオブジェクトのキーと配列の添字 ("0", "1", …) の列。 壊れた JSON でも
/// 読めた所までの索引を返す (行が分からなければ 0)。

#include <map>
#include <string>
#include <vector>

namespace ergo::package_scan {

class JsonLineIndex {
public:
    static JsonLineIndex build(const std::string& text);

    /// 値の始まる行 (1 始まり)。 無ければ 0。
    int line_of(const std::vector<std::string>& path) const;

private:
    std::map<std::string, int> lines_;
};

} // namespace ergo::package_scan
