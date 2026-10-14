#include "source.h"

#include "ergo/package_scan/git_index_reader.h"

namespace ergo::package_scan::detail {

namespace {

std::string join_rel(const std::string& dir, const std::string& file) {
    return dir.empty() ? file : dir + "/" + file;
}

void read_package(Source& s, std::vector<std::string>& notes) {
    const std::string rel = join_rel(s.package_dir, "package.json");
    std::string text;
    if (!s.scope.read(rel, text)) return;
    std::string error;
    s.package_file = s.scope.display(rel);
    if (!parse_package_json(text, s.package, &error)) {
        notes.push_back(s.package_file + ": " + error);
        return;
    }
    s.has_package = true;
}

void read_git(Source& s, std::vector<std::string>& notes) {
    std::string text;
    if (!s.scope.read(".gitmodules", text)) return;
    s.gitmodules_file = s.scope.display(".gitmodules");
    s.submodules      = parse_gitmodules(text);
    if (s.submodules.empty()) return;

    // gitlink の SHA は .git がチェックアウトの中のディレクトリのときだけ読む
    // (.git がファイルの worktree は git ディレクトリが範囲外にあるので読まない)。
    if (!s.scope.is_directory(".git")) {
        notes.push_back(s.scope.display(".git") + ": not a directory inside the checkout — gitlink unread");
        return;
    }
    std::string index;
    if (!s.scope.read(".git/index", index)) return;
    std::string error;
    if (!parse_index_gitlinks(index, s.gitlinks, &error)) {
        notes.push_back(s.scope.display(".git/index") + ": " + error);
        return;
    }
    s.gitlinks_known = true;
}

} // namespace

void read_source(Source& s, std::vector<std::string>& notes) {
    read_package(s, notes);
    if (s.read_git) read_git(s, notes);
}

} // namespace ergo::package_scan::detail
