#include "ergo/studio/ingress/remote_scope.h"

#include <array>

namespace ergo::studio::ingress {
namespace {

struct LocalOnly {
    const char* path;
    bool        subtree;   ///< also everything under "<path>/"
};

// Keep in step with mutation_guard's users and LauncherRoutes (spec/feature/tool/ergo.md).
constexpr std::array<LocalOnly, 6> kLocalOnly{{
    {"/api/launcher/add", false},
    {"/api/launcher/remove", false},
    {"/studio", true},
    {"/lesson", true},
    {"/git", true},
    {"/asset_import", true},
}};

bool reading(const std::string& method) { return method == "GET" || method == "HEAD" || method == "OPTIONS"; }

bool covers(const LocalOnly& rule, const std::string& path) {
    const std::string base = rule.path;
    if (path == base) return true;
    return rule.subtree && path.size() > base.size() && path.compare(0, base.size(), base) == 0 && path[base.size()] == '/';
}

} // namespace

std::optional<std::string> remote_refusal(const std::string& method, const std::string& path) {
    if (reading(method)) return std::nullopt;
    for (const LocalOnly& rule : kLocalOnly) {
        if (covers(rule, path)) return method + " " + path + " can only be done from this machine";
    }
    return std::nullopt;
}

} // namespace ergo::studio::ingress
