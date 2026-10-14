#include "ref_source_router.h"

#include <utility>

namespace ergo::package_cli {

RefSourceRouter::RefSourceRouter(std::string sibling_root, std::string cache_dir)
    : sibling_(std::move(sibling_root)), remote_(std::move(cache_dir)) {}

bool RefSourceRouter::use_sibling(const pw::RepoRef& repo) const {
    return !sibling_.checkout_dir(repo).empty();
}

pw::RefListing RefSourceRouter::list_refs(const pw::RepoRef& repo) {
    return use_sibling(repo) ? sibling_.list_refs(repo) : remote_.list_refs(repo);
}

bool RefSourceRouter::sync_history(const pw::RepoRef& repo) {
    return use_sibling(repo) ? sibling_.sync_history(repo) : remote_.sync_history(repo);
}

std::optional<int> RefSourceRouter::count_commits(const pw::RepoRef& repo, const std::string& from,
                                                  const std::string& to) {
    return use_sibling(repo) ? sibling_.count_commits(repo, from, to) : remote_.count_commits(repo, from, to);
}

std::optional<bool> RefSourceRouter::is_ancestor(const pw::RepoRef& repo, const std::string& ancestor,
                                                 const std::string& descendant) {
    return use_sibling(repo) ? sibling_.is_ancestor(repo, ancestor, descendant)
                             : remote_.is_ancestor(repo, ancestor, descendant);
}

} // namespace ergo::package_cli
