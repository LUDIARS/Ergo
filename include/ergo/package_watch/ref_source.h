#pragma once

// 更新の取得元 (spec/feature/package-manager.md §2.3)。 ライブラリはこの interface だけを持ち、
// git プロセスは tools/package_cli のアダプタに置く (テストは偽ソース)。
//
//   IRefSource      タグと追跡ブランチの先頭 (git ls-remote --tags --heads、 または兄弟チェックアウトの
//                   refs/remotes/origin/* と refs/tags/*。 兄弟は fetch しない)
//   IHistorySource  距離と祖先判定 (ツリーレスの bare ミラー、 または兄弟チェックアウトの履歴)。
//                   答えられなければ nullopt (距離は空欄になる)

#include <map>
#include <optional>
#include <string>

namespace ergo::package_watch {

struct RepoRef {
    std::string name;        ///< 依存の名前
    std::string repository;  ///< 取得元
};

enum class RefOrigin {
    Remote,   ///< ls-remote (今の値)
    Sibling,  ///< 兄弟チェックアウト (最後に fetch した時点の値)
};

struct RefListing {
    bool                               observed = false;
    std::string                        reason;      ///< observed == false の理由
    RefOrigin                          origin = RefOrigin::Remote;
    std::map<std::string, std::string> heads;       ///< ブランチ名 → commit SHA
    std::map<std::string, std::string> tags;        ///< タグ名 → commit SHA (注釈つきタグは剥がした値)
    std::string                        local_head;  ///< 兄弟チェックアウトの HEAD (Remote では "")
};

class IRefSource {
public:
    virtual ~IRefSource() = default;
    virtual RefListing list_refs(const RepoRef& repo) = 0;
};

class IHistorySource {
public:
    virtual ~IHistorySource() = default;
    /// ミラーを取得元に揃える (fetch)。 ミラーが無い・失敗なら false。 兄弟チェックアウトには呼ばれない。
    virtual bool sync_history(const RepoRef& repo) = 0;
    /// from..to のコミット数 (rev-list --count)。
    virtual std::optional<int> count_commits(const RepoRef& repo, const std::string& from,
                                             const std::string& to) = 0;
    /// ancestor が descendant の祖先 (同じ commit を含む) か。
    virtual std::optional<bool> is_ancestor(const RepoRef& repo, const std::string& ancestor,
                                            const std::string& descendant) = 0;
};

} // namespace ergo::package_watch
