#pragma once
/// The launcher's cache of recently opened projects (<studio home>/recent_projects.json):
///   {"version": 1, "projects": [{"source", "name", "root", "style", "domain", "last_opened"}]}
/// `source` (the profile file's absolute path) identifies a project. The list
/// is in recency order (front = the last opened or added). Pure text <-> value
/// functions; reading and writing the file is ProjectCatalog's.
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::project {

struct RecentProject {
    std::string source;        ///< profile file, absolute (the key)
    std::string name;          ///< profile "name" when it was last read
    std::string root;          ///< profile root when it was last read
    std::string style;         ///< project_style.h name when it was last read ("" = unknown)
    std::string domain;        ///< the profile's "domain" when it was last read (free text, "" = none)
    std::string last_opened;   ///< UTC ISO-8601; "" = added but never opened
};

/// How many projects the cache keeps (Godot / Unity Hub keep a short list too).
inline constexpr std::size_t kMaxRecentProjects = 30;

/// The list in the file's order, duplicates (same source) merged into the
/// first, cut to kMaxRecentProjects. nullopt with `err` when the text is not
/// such a file (the caller treats it as empty and leaves the file alone).
std::optional<std::vector<RecentProject>> parse_recent_projects(const std::string& text, std::string& err);

/// The file's text (version 1), in the list's order.
std::string serialize_recent_projects(const std::vector<RecentProject>& list);

/// Put `p` first: an entry with the same source is replaced, the list is cut to `max`.
void remember_project(std::vector<RecentProject>& list, RecentProject p, std::size_t max = kMaxRecentProjects);

/// Drop the entry with `source`; false when there was none.
bool forget_project(std::vector<RecentProject>& list, const std::string& source);

} // namespace ergo::studio::project
