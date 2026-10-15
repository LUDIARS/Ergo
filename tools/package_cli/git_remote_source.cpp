#include "git_remote_source.h"

#include "file_io.h"
#include "git_history.h"
#include "process.h"

#include "ergo/package_watch/ref_listing.h"
#include "ergo/package_watch/watch_dependency.h"

#include <filesystem>
#include <utility>

namespace ergo::package_cli {

GitRemoteSource::GitRemoteSource(std::string cache_dir) : cache_dir_(std::move(cache_dir)) {}

pw::RefListing GitRemoteSource::list_refs(const pw::RepoRef& repo) {
    pw::RefListing out;
    out.origin = pw::RefOrigin::Remote;
    if (!repo.repository.empty() && repo.repository.front() == '-') {
        out.reason = "repository must not start with '-'";
        return out;
    }
    if (!quote_safe(repo.repository)) {
        out.reason = "repository cannot be passed to git";
        return out;
    }
    std::string text;
    if (!capture({"git", "ls-remote", "--tags", "--heads", "--", repo.repository}, text)) {
        out.reason = "git ls-remote failed (offline or not reachable)";
        return out;
    }
    pw::parse_ref_lines(text, pw::RefLayout::Remote, out);
    out.observed = true;
    return out;
}

std::string GitRemoteSource::mirror_dir(const pw::RepoRef& repo) const {
    if (cache_dir_.empty()) return {};
    const std::string name = pw::repository_name(repo.repository);
    if (name.empty()) return {};
    const std::string dir = join(cache_dir_, "mirrors/" + name + ".git");
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::u8path(join(dir, "HEAD")), ec) ? dir : std::string();
}

bool GitRemoteSource::sync_history(const pw::RepoRef& repo) {
    const std::string dir = mirror_dir(repo);
    if (dir.empty()) return false;
    // clone --bare は fetch の refspec を持たないので明示する。 origin 経由にして partial clone の filter を保つ。
    // 標準出力は表 / --json のために空けておく (capture で捨てる)。
    std::string ignored;
    return capture({"git", "--git-dir", dir, "fetch", "--quiet", "--prune", "origin",
                    "+refs/heads/*:refs/heads/*", "+refs/tags/*:refs/tags/*"},
                   ignored);
}

std::optional<int> GitRemoteSource::count_commits(const pw::RepoRef& repo, const std::string& from,
                                                  const std::string& to) {
    const std::string dir = mirror_dir(repo);
    if (dir.empty()) return std::nullopt;
    return git_count_commits({"git", "--git-dir", dir}, from, to);
}

std::optional<bool> GitRemoteSource::is_ancestor(const pw::RepoRef& repo, const std::string& ancestor,
                                                 const std::string& descendant) {
    const std::string dir = mirror_dir(repo);
    if (dir.empty()) return std::nullopt;
    return git_is_ancestor({"git", "--git-dir", dir}, ancestor, descendant);
}

} // namespace ergo::package_cli
