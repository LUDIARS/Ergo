#pragma once
/// Path containment guard for plugins that read / write under a configured
/// root (visus, ui_layout). Port of tools/ergo core/safe_path.ts.
#include <filesystem>
#include <optional>
#include <string>

namespace ergo::studio::plugins {

/// Resolve `rel` (UTF-8, may be absolute) against `root` lexically and
/// return the absolute path, or nullopt when it is not inside `root` (the
/// root itself counts as inside). Rejects "..", a path that climbs out, and
/// a path on another drive / UNC share. Comparison is lexical and
/// case-sensitive, so a differently cased root is rejected (the safe side).
std::optional<std::filesystem::path> resolve_within(const std::filesystem::path& root, const std::string& rel);

/// `p` relative to `root` with "/" separators (UTF-8).
std::string relative_slash(const std::filesystem::path& root, const std::filesystem::path& p);

} // namespace ergo::studio::plugins
