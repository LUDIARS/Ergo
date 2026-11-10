#pragma once
/// Decisions for fetching Curare versions into a project (port of
/// studio/domain/resource-sync.ts, ST-INV-07). Curare's plan is
/// overwrite:false: a local file with the same name but other content stays
/// and is reported as a conflict for a person to resolve. Pure.
#include <filesystem>
#include <optional>
#include <string>

namespace ergo::studio::plugins::studio {

enum class SyncAction { SkipSame, Download, Conflict, Unavailable };
const char* sync_action_name(SyncAction a);   ///< "skip_same", "download", "conflict", "unavailable"

SyncAction decide_sync_action(const std::string& content_hash, bool needs_download, bool has_download_url,
                              const std::optional<std::string>& local_hash);

/// A Curare resource name as a local file name: separators and Windows-
/// reserved characters become '_', reserved device names and "." / ".." are
/// refused (nullopt), at most 200 characters.
std::optional<std::string> safe_file_name(const std::string& name);

/// `subdir` (relative) inside `repo`, else nullopt (absolute or escaping).
std::optional<std::filesystem::path> resolve_inside(const std::filesystem::path& repo, const std::string& subdir);

/// "sha256:<lower-case hex>".
std::string to_content_hash(const std::string& hex_digest);

} // namespace ergo::studio::plugins::studio
