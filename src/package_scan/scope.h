#pragma once

// ergo_package_scan の内部部品: 読んでよい範囲 (1 つのチェックアウト) とその中のファイルの読み取り。
//
// 範囲の外 (正規化した結果が根の外に出るパス、 絶対パス、 ".." を含む path_hint) は開かない。
// 開いたファイルは表示名 (根拠に載せる名前) で `files_read` に記録する。

#include <filesystem>
#include <string>
#include <vector>

namespace ergo::package_scan::detail {

/// '/' 区切りの相対パスとして安全か (空でなく、 絶対でなく、 ".." も ':' も含まない)。
bool is_safe_relative(const std::string& rel);

/// `path` が `base` の中 (同じも含む) か。 どちらも正規化済みの絶対パス。
bool is_within(const std::filesystem::path& base, const std::filesystem::path& path);

/// UTF-8 の '/' 区切り文字列にする (C++17 / C++20 の u8string の型差を吸収)。
std::string generic_utf8(const std::filesystem::path& path);

/// 正規化した絶対パス。
std::filesystem::path normalized(const std::filesystem::path& path);

class Scope {
public:
    /// `base` は読んでよい範囲の根、 `display` は根拠に載せる接頭辞 (例: "Thaleia" / "ergo:")。
    Scope(std::filesystem::path base, std::string display, std::vector<std::string>* files_read);

    const std::filesystem::path& base() const { return base_; }

    /// 範囲内の相対パスの表示名 (例: "Thaleia/package.json")。
    std::string display(const std::string& rel) const;

    /// 範囲内のファイルを読む。 範囲外・無い・開けないなら false (記録しない)。
    bool read(const std::string& rel, std::string& out) const;

    /// 範囲内にディレクトリとしてあるか (読まない)。
    bool is_directory(const std::string& rel) const;

private:
    std::filesystem::path     base_;
    std::string               display_;
    std::vector<std::string>* files_read_;
};

} // namespace ergo::package_scan::detail
