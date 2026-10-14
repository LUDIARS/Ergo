#pragma once

// ergo_pkg watch / outdated のアダプタ: リポごとに取得元を選ぶ。 兄弟チェックアウトがあればそれ
// (fetch しない)、 無ければ ls-remote + ミラー。

#include "git_remote_source.h"
#include "git_sibling_source.h"

#include <string>

namespace ergo::package_cli {

class RefSourceRouter : public pw::IRefSource, public pw::IHistorySource {
public:
    RefSourceRouter(std::string sibling_root, std::string cache_dir);

    pw::RefListing      list_refs(const pw::RepoRef& repo) override;
    bool                sync_history(const pw::RepoRef& repo) override;
    std::optional<int>  count_commits(const pw::RepoRef& repo, const std::string& from,
                                      const std::string& to) override;
    std::optional<bool> is_ancestor(const pw::RepoRef& repo, const std::string& ancestor,
                                    const std::string& descendant) override;

private:
    bool use_sibling(const pw::RepoRef& repo) const;

    GitSiblingSource sibling_;
    GitRemoteSource  remote_;
};

} // namespace ergo::package_cli
