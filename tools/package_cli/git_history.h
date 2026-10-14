#pragma once

// ergo_pkg watch / outdated のアダプタ部品: git の履歴への問い合わせ (距離と祖先判定)。
// ミラー (`git --git-dir <mirror>`) と兄弟チェックアウト (`git -C <dir>`) で共通。

#include <optional>
#include <string>
#include <vector>

namespace ergo::package_cli {

/// git_prefix は {"git", "--git-dir", <dir>} か {"git", "-C", <dir>}。
/// from..to のコミット数。 取れなければ nullopt。
std::optional<int> git_count_commits(const std::vector<std::string>& git_prefix, const std::string& from,
                                     const std::string& to);

/// `merge-base --is-ancestor`: 0 → true、 1 → false、 それ以外 (知らない commit など) → nullopt。
std::optional<bool> git_is_ancestor(const std::vector<std::string>& git_prefix, const std::string& ancestor,
                                    const std::string& descendant);

} // namespace ergo::package_cli
