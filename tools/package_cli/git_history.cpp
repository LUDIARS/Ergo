#include "git_history.h"

#include "process.h"

#include "ergo/package_manager/declaration_check.h"

#include <cctype>

namespace ergo::package_cli {

namespace {

std::vector<std::string> with(std::vector<std::string> argv, std::initializer_list<std::string> more) {
    argv.insert(argv.end(), more.begin(), more.end());
    return argv;
}

bool both_shas(const std::string& a, const std::string& b) {
    return ergo::package_manager::is_full_sha(a) && ergo::package_manager::is_full_sha(b);
}

} // namespace

std::optional<int> git_count_commits(const std::vector<std::string>& git_prefix, const std::string& from,
                                     const std::string& to) {
    if (!both_shas(from, to)) return std::nullopt;
    std::string out;
    if (!capture(with(git_prefix, {"rev-list", "--count", from + ".." + to}), out)) return std::nullopt;
    int n = 0;
    bool any = false;
    for (const char c : out) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            n = n * 10 + (c - '0');
            any = true;
        } else if (any) {
            break;
        }
    }
    return any ? std::optional<int>(n) : std::nullopt;
}

std::optional<bool> git_is_ancestor(const std::vector<std::string>& git_prefix, const std::string& ancestor,
                                    const std::string& descendant) {
    if (!both_shas(ancestor, descendant)) return std::nullopt;
    std::string out;
    const int rc = capture_status(with(git_prefix, {"merge-base", "--is-ancestor", ancestor, descendant}), out);
    if (rc == 0) return true;
    if (rc == 1) return false;
    return std::nullopt;
}

} // namespace ergo::package_cli
