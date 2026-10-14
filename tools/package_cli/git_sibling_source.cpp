#include "git_sibling_source.h"

#include "file_io.h"
#include "git_history.h"
#include "process.h"

#include "ergo/package_watch/ref_listing.h"
#include "ergo/package_watch/watch_dependency.h"

#include <filesystem>
#include <utility>

namespace ergo::package_cli {

GitSiblingSource::GitSiblingSource(std::string root) : root_(std::move(root)) {}

std::string GitSiblingSource::checkout_dir(const pw::RepoRef& repo) const {
    if (root_.empty()) return {};
    const std::string name = pw::repository_name(repo.repository);
    if (name.empty() || name == "." || name == "..") return {};
    const std::string dir = join(root_, name);
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::u8path(join(dir, ".git")), ec) ? dir : std::string();
}

pw::RefListing GitSiblingSource::list_refs(const pw::RepoRef& repo) {
    pw::RefListing out;
    out.origin = pw::RefOrigin::Sibling;
    const std::string dir = checkout_dir(repo);
    if (dir.empty()) {
        out.reason = "no sibling checkout";
        return out;
    }
    std::string text;
    if (!capture({"git", "-C", dir, "show-ref", "--dereference"}, text)) {
        out.reason = "sibling checkout refs could not be read";
        return out;
    }
    pw::parse_ref_lines(text, pw::RefLayout::Sibling, out);
    out.local_head = git_head(dir);
    out.observed   = true;
    return out;
}

bool GitSiblingSource::sync_history(const pw::RepoRef&) { return false; }

std::optional<int> GitSiblingSource::count_commits(const pw::RepoRef& repo, const std::string& from,
                                                   const std::string& to) {
    const std::string dir = checkout_dir(repo);
    if (dir.empty()) return std::nullopt;
    return git_count_commits({"git", "-C", dir}, from, to);
}

std::optional<bool> GitSiblingSource::is_ancestor(const pw::RepoRef& repo, const std::string& ancestor,
                                                  const std::string& descendant) {
    const std::string dir = checkout_dir(repo);
    if (dir.empty()) return std::nullopt;
    return git_is_ancestor({"git", "-C", dir}, ancestor, descendant);
}

} // namespace ergo::package_cli
