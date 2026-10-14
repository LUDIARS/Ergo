#pragma once

// `update --to <tag|main|sha>` (と `add` の非公開 rev) の解決 (spec/feature/package-manager.md §2.2)。
//
//   tag    → 注釈タグを剥いた commit。 expect_tag にそのタグ
//   main   → 追跡ブランチの先頭。 タグの無い pin なので STRICT で通らない旨を警告し、 expect_tag は外す
//   sha    → その commit (40 桁)。 main と同じくタグの無い pin
//   その他 → ブランチ名として先頭を取る (feature ブランチ)
//
// main の祖先でない commit (squash で消える feature の SHA) は allow_off_main が無ければ拒む。
// git への問い合わせは IRevResolver に任せる (CLI がプロセスで実装し、 テストは偽物)。 ネットワークは CLI 側だけ。

#include <string>
#include <vector>

namespace ergo::package_manager {

/// 1 つのリポへの問い合わせ。 取れない (通信・git の失敗) なら false を返し、 結果は使わない。
class IRevResolver {
public:
    virtual ~IRevResolver() = default;
    /// タグの commit (注釈タグは剥く)。 found はタグがあるか。
    virtual bool tag_commit(const std::string& tag, bool& found, std::string& sha) = 0;
    /// ブランチの先頭。 found はブランチがあるか。
    virtual bool branch_head(const std::string& branch, bool& found, std::string& sha) = 0;
    /// commit を持っているか。
    virtual bool has_commit(const std::string& sha, bool& found) = 0;
    /// ancestor が descendant の祖先 (同じも含む) か。
    virtual bool is_ancestor(const std::string& ancestor, const std::string& descendant, bool& yes) = 0;
};

enum class ResolveStatus {
    Ok,
    InputError,  ///< 終了 2 (知らない参照・off-main の拒否)
    IoError,     ///< 終了 3 (取れない)
};

struct RevRequest {
    std::string to;                     ///< --to
    std::string track_branch = "main";  ///< 追跡ブランチ
    bool        allow_off_main = false;
};

struct RevResolution {
    ResolveStatus            status = ResolveStatus::Ok;
    std::string              error;
    std::string              rev;         ///< 40 桁 SHA
    std::string              expect_tag;  ///< 空ならタグの無い pin (expect_tag を外す)
    bool                     off_main = false;
    std::vector<std::string> warnings;    ///< STRICT・off-main の警告 (人向け)
};

RevResolution resolve_rev(const RevRequest& request, IRevResolver& resolver);

} // namespace ergo::package_manager
