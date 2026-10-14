#pragma once

// タグ名の版 ("v1.2.3"、 "v1.2"、 "v1.2.3-rc.1") の解析と比較。 先頭の 'v' が要る (v* タグだけを見る)。

#include <optional>
#include <string>
#include <vector>

namespace ergo::package_watch {

struct Semver {
    std::vector<int> numbers;  ///< 1〜3 個 (足りない分は 0 とみなす)
    std::string      pre;      ///< '-' 以降 (無ければ "")
};

std::optional<Semver> parse_semver(const std::string& tag);

/// a < b なら負、 同じなら 0、 a > b なら正。 pre-release は同じ番号の正式版より前。
int compare_semver(const Semver& a, const Semver& b);

} // namespace ergo::package_watch
