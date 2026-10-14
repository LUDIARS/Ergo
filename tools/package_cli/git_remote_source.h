#pragma once

// ergo_pkg watch / outdated のアダプタ: 取得元へ `git ls-remote --tags --heads` を 1 往復、 距離と祖先判定は
// 任意のツリーレス bare ミラー `<cache>/mirrors/<Repo>.git` (spec/feature/package-manager.md §2.3)。
// ミラーは作らない (`git clone --bare --filter=tree:0 <repository> <cache>/mirrors/<Repo>.git` で用意する)。
// 無ければ距離と祖先判定は nullopt。 git の資格情報は利用者の git に任せる。

#include "ergo/package_watch/ref_source.h"

#include <string>

namespace ergo::package_cli {

namespace pw = ergo::package_watch;

class GitRemoteSource : public pw::IRefSource, public pw::IHistorySource {
public:
    /// cache_dir が "" ならミラーを使わない。
    explicit GitRemoteSource(std::string cache_dir);

    pw::RefListing      list_refs(const pw::RepoRef& repo) override;
    bool                sync_history(const pw::RepoRef& repo) override;
    std::optional<int>  count_commits(const pw::RepoRef& repo, const std::string& from,
                                      const std::string& to) override;
    std::optional<bool> is_ancestor(const pw::RepoRef& repo, const std::string& ancestor,
                                    const std::string& descendant) override;

    /// <cache>/mirrors/<Repo>.git (ミラーがあるときだけ。 無ければ "")。
    std::string mirror_dir(const pw::RepoRef& repo) const;

private:
    std::string cache_dir_;
};

} // namespace ergo::package_cli
