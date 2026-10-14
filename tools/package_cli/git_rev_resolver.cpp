#include "git_rev_resolver.h"

#include "file_io.h"
#include "process.h"

#include <cstdlib>
#include <filesystem>

namespace ergo::package_cli {

namespace fs = std::filesystem;

namespace {

/// 取得元 URL の末尾名 (`.git` を除く)。 兄弟チェックアウトのディレクトリ名と同じ規則。
std::string repo_name(const std::string& repository) {
    std::string s = repository;
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    const auto slash = s.find_last_of("/\\:");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    if (s.size() > 4 && s.compare(s.size() - 4, 4, ".git") == 0) s.resize(s.size() - 4);
    return s.empty() ? "repo" : s;
}

std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

/// git に渡す参照名として安全か (選択肢に見える '-' 始まりと git の禁止文字を拒む)。
bool safe_ref(const std::string& name) {
    if (name.empty() || name[0] == '-') return false;
    for (const char c : name) {
        if (c == ' ' || c == '~' || c == '^' || c == ':' || c == '?' || c == '*' || c == '[' || c == '\\') return false;
    }
    return name.find("..") == std::string::npos;
}

} // namespace

std::string mirror_root(const std::string& cache_arg) {
    if (!cache_arg.empty()) return cache_arg;
    for (const char* var : {"ERGO_PKG_CACHE_DIR", "ERGO_DEPENDENCY_CACHE_DIR"}) {
        if (const char* v = std::getenv(var); v && *v) return v;
    }
    std::error_code ec;
    const auto tmp = fs::temp_directory_path(ec);
    return ec ? std::string() : (tmp / "ergo_pkg").generic_u8string();
}

GitRevResolver::GitRevResolver(std::string repository, std::string root)
    : repository_(std::move(repository)),
      mirror_(join(join(root, "mirrors"), repo_name(repository_) + ".git")) {}

bool GitRevResolver::prepare(std::string& error) {
    if (repository_.empty() || repository_[0] == '-') {
        error = "the package has no repository to resolve against";
        return false;
    }
    std::error_code ec;
    bool ok = false;
    if (fs::is_directory(fs::u8path(mirror_), ec)) {
        ok = run({"git", "--git-dir", mirror_, "fetch", "--quiet", "--prune", "--tags", "origin",
                  "+refs/heads/*:refs/heads/*"});
    } else {
        fs::create_directories(fs::u8path(mirror_).parent_path(), ec);
        ok = run({"git", "clone", "--quiet", "--bare", "--filter=tree:0", repository_, mirror_});
    }
    if (!ok) error = "cannot fetch " + repository_ + " into the mirror " + mirror_;
    ready_ = ok;
    return ok;
}

bool GitRevResolver::ref_commit(const std::string& ref, bool& found, std::string& sha) {
    if (!ready_) return false;
    const int rc = status({"git", "--git-dir", mirror_, "show-ref", "--verify", "--quiet", ref});
    if (rc < 0) return false;
    found = rc == 0;
    if (!found) return true;
    std::string out;
    if (!capture({"git", "--git-dir", mirror_, "rev-list", "-n", "1", ref}, out)) return false;
    sha = trimmed(out);
    return true;
}

bool GitRevResolver::tag_commit(const std::string& tag, bool& found, std::string& sha) {
    if (!safe_ref(tag)) {
        found = false;
        return ready_;
    }
    return ref_commit("refs/tags/" + tag, found, sha);
}

bool GitRevResolver::branch_head(const std::string& branch, bool& found, std::string& sha) {
    if (!safe_ref(branch)) {
        found = false;
        return ready_;
    }
    return ref_commit("refs/heads/" + branch, found, sha);
}

bool GitRevResolver::has_commit(const std::string& sha, bool& found) {
    if (!ready_) return false;
    std::string out;
    found = capture({"git", "--git-dir", mirror_, "cat-file", "-t", sha}, out) && trimmed(out) == "commit";
    return true;
}

bool GitRevResolver::is_ancestor(const std::string& ancestor, const std::string& descendant, bool& yes) {
    if (!ready_) return false;
    const int rc = status({"git", "--git-dir", mirror_, "merge-base", "--is-ancestor", ancestor, descendant});
    if (rc != 0 && rc != 1) return false;
    yes = rc == 0;
    return true;
}

} // namespace ergo::package_cli
