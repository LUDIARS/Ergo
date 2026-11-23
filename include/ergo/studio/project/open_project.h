#pragma once
/// The project the Studio has open: none or one, and a revision bumped on
/// every open / close so readers (the declaration, the plugins' profile
/// source) can tell that it changed. Thread safe: HTTP threads open and close,
/// the UI thread and plugins read.
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace ergo::studio::project {

struct ProjectRef {
    std::string source;   ///< profile file, absolute
    std::string name;
    std::string root;
    std::string style;    ///< project_style.h name ("" = unknown)
    std::string domain;   ///< the profile's "domain" (free text: lesson, game, cglive, industrial, ...; "" = none)
};

class OpenProject {
public:
    std::optional<ProjectRef> current() const;
    uint64_t                  revision() const { return revision_.load(); }

    void open(ProjectRef project);
    /// False when nothing was open.
    bool close();

private:
    mutable std::mutex        mutex_;
    std::optional<ProjectRef> current_;
    std::atomic<uint64_t>     revision_{0};
};

} // namespace ergo::studio::project
