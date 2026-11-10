#include "safe_path.h"

#include <algorithm>
#include <system_error>

namespace ergo::studio::plugins {
namespace fs = std::filesystem;

std::optional<fs::path> resolve_within(const fs::path& root, const std::string& rel) {
    std::error_code ec;
    const fs::path base = fs::absolute(root, ec).lexically_normal();
    if (ec) return std::nullopt;
    fs::path target = (base / fs::u8path(rel)).lexically_normal();
    // "dir/" normalises to "dir/" with an empty filename; drop it so the root compares equal.
    if (!target.empty() && target.filename().empty() && target != target.root_path()) target = target.parent_path();
    fs::path b = base;
    if (!b.empty() && b.filename().empty() && b != b.root_path()) b = b.parent_path();

    const fs::path back = target.lexically_relative(b);
    // Empty: no relative path exists (another drive / root name).
    if (back.empty() || back.is_absolute()) return std::nullopt;
    const auto first = back.begin();
    if (first != back.end() && *first == "..") return std::nullopt;
    return target;
}

std::string relative_slash(const fs::path& root, const fs::path& p) {
    std::string s = p.lexically_relative(root).u8string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

} // namespace ergo::studio::plugins
