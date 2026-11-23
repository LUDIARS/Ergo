#pragma once
/// recent_projects.json on disk: load (a missing file is an empty list) and
/// save (a temporary file, then a rename over the original).
///
/// A file that cannot be read as the cache is treated as empty and marked
/// broken; a broken catalog never saves, so the original stays as it was for
/// a person to look at (the list still changes in memory for this run).
#include "ergo/studio/project/recent_projects.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ergo::studio::project {

class ProjectCatalog {
public:
    /// `file` empty: memory only (no Studio home), load and save do nothing.
    explicit ProjectCatalog(std::filesystem::path file);

    /// False with `err` when the file exists but is not a cache (then broken()).
    bool load(std::string& err);
    /// False with `err` when broken() or the file could not be written.
    bool save(std::string& err) const;

    const std::vector<RecentProject>& projects() const { return projects_; }
    void remember(RecentProject p) { remember_project(projects_, std::move(p)); }
    bool forget(const std::string& source) { return forget_project(projects_, source); }

    bool                         broken() const { return broken_; }
    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path      file_;
    std::vector<RecentProject> projects_;
    bool                       broken_ = false;
};

} // namespace ergo::studio::project
