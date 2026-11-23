#pragma once
/// The launcher's model (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A):
/// the rows it lists, and open / close / add / remove. The recent projects
/// come first (cache order), then the profiles found on disk that are not in
/// the cache. Opening records the project first in the cache and makes it the
/// OpenProject; nothing is opened on start.
///
/// How a profile file is read and where profiles are found are given from
/// outside (LauncherDeps), so the library does not know the profile format.
/// Thread safe (HTTP threads and the UI thread).
#include "ergo/studio/project/open_project.h"
#include "ergo/studio/project/project_catalog.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::project {

struct LauncherRow {
    std::string source, name, root;
    std::string style;            ///< project_style.h name ("" = unknown)
    std::string domain;           ///< the profile's "domain" ("" = none)
    std::string last_opened;      ///< "" = never opened (found, or added)
    bool        recent  = false;  ///< in the cache
    bool        missing = false;  ///< the profile file is gone
    bool        open    = false;  ///< the open project
};

struct LauncherDeps {
    /// Name, root, style and domain of the profile at `source` (absolute), or nullopt with `err`.
    std::function<std::optional<ProjectRef>(const std::filesystem::path& source, std::string& err)> probe;
    /// Profiles found on disk (profiles/ and the plugin folders), in a stable order.
    std::function<std::vector<ProjectRef>()> discover;
    /// Now, UTC ISO-8601.
    std::function<std::string()> now;
};

class Launcher {
public:
    /// Loads the cache (a broken one shows as notice()).
    Launcher(ProjectCatalog catalog, OpenProject& open, LauncherDeps deps);

    std::vector<LauncherRow> rows() const;

    /// Read the profile, put it first in the cache, make it the open project.
    bool open(const std::string& source, std::string& err);
    /// Back to the launcher. False when nothing was open.
    bool close();
    /// Add a profile to the list without opening it (Unity Hub "Add").
    bool add(const std::string& source, std::string& err);
    /// Take it off the list (the file stays). Closes it when it is open.
    bool remove(const std::string& source, std::string& err);

    const OpenProject& project() const { return open_; }
    /// Bumped by every change of the rows or of the open project.
    uint64_t           revision() const { return revision_.load() + open_.revision(); }
    /// The last problem worth showing ("" = none): a broken cache, a failed save.
    std::string        notice() const;

    /// `source` made absolute and normal ("/"-separated), the key of a project.
    static std::string normalize(const std::string& source);

private:
    void save_locked();

    mutable std::mutex    mutex_;
    ProjectCatalog        catalog_;
    OpenProject&          open_;
    LauncherDeps          deps_;
    std::string           notice_;
    std::atomic<uint64_t> revision_{0};
};

} // namespace ergo::studio::project
