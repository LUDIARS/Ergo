#pragma once

// ergo_pkg watch / outdated のアダプタ: 兄弟チェックアウト `<root>/<Repo>` (ERGO_PACKAGES_SIBLING_ROOT / --root)
// の refs/remotes/origin/* と refs/tags/* を読む (spec/feature/package-manager.md §2.3)。 fetch はしない。
// 値は「最後に fetch した時点」で、 `.worktrees` は見ない (<root>/<Repo> そのものだけ)。

#include "ergo/package_watch/ref_source.h"

#include <string>

namespace ergo::package_cli {

namespace pw = ergo::package_watch;

class GitSiblingSource : public pw::IRefSource, public pw::IHistorySource {
public:
    /// root が "" なら兄弟チェックアウトは無い。
    explicit GitSiblingSource(std::string root);

    /// <root>/<Repo> が git のチェックアウトなら、 そのディレクトリ。 無ければ ""。
    std::string checkout_dir(const pw::RepoRef& repo) const;

    pw::RefListing      list_refs(const pw::RepoRef& repo) override;
    /// 兄弟チェックアウトは fetch しない (常に false)。
    bool                sync_history(const pw::RepoRef& repo) override;
    std::optional<int>  count_commits(const pw::RepoRef& repo, const std::string& from,
                                      const std::string& to) override;
    std::optional<bool> is_ancestor(const pw::RepoRef& repo, const std::string& ancestor,
                                    const std::string& descendant) override;

private:
    std::string root_;
};

} // namespace ergo::package_cli
