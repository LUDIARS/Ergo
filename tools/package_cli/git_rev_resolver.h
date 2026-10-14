#pragma once

// IRevResolver の git 実装 (update / add の版の解決)。 リポごとにツリーレスの bare ミラー
// `<cache>/mirrors/<Repo>.git` (spec/feature/package-manager.md §2.3) を作るか fetch してから、 そのミラーに問い合わせる。
// <cache> は --cache → 環境変数 ERGO_PKG_CACHE_DIR → ERGO_DEPENDENCY_CACHE_DIR → 一時ディレクトリの ergo_pkg。
// どれもリポの外 (§8)。 資格情報は利用者の git に任せる。

#include "ergo/package_manager/rev_resolution.h"

#include <string>

namespace ergo::package_cli {

/// ミラーの置き場所の根 (--cache の値、 空なら環境変数と一時ディレクトリ)。
std::string mirror_root(const std::string& cache_arg);

class GitRevResolver : public ergo::package_manager::IRevResolver {
public:
    GitRevResolver(std::string repository, std::string root);

    /// ミラーを作る / 更新する (ネットワーク)。 失敗したら false と理由。
    bool prepare(std::string& error);

    bool tag_commit(const std::string& tag, bool& found, std::string& sha) override;
    bool branch_head(const std::string& branch, bool& found, std::string& sha) override;
    bool has_commit(const std::string& sha, bool& found) override;
    bool is_ancestor(const std::string& ancestor, const std::string& descendant, bool& yes) override;

private:
    bool ref_commit(const std::string& ref, bool& found, std::string& sha);

    std::string repository_;
    std::string mirror_;
    bool        ready_ = false;
};

} // namespace ergo::package_cli
