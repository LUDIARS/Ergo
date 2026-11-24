#pragma once
/// What the launcher's window needs from the remote dev server: start it for a
/// project, stop it, and show where it answers
/// (spec/plan/2026-11-23-studio-launcher-and-devserver.md, B). The launcher's
/// view depends on this interface only (the web server sits behind it).
#include <cstdint>
#include <string>
#include <vector>

namespace ergo::studio::project {

struct DevServerStatus {
    bool                     running = false;
    std::string              source;         ///< the project served ("" = none: --headless without --project)
    std::string              local_url;      ///< e.g. http://127.0.0.1:5170/shell/ ("" = stopped)
    std::string              remote_target;  ///< 127.0.0.1:<M> the tunnel / tailscale serve forwards to ("" = off)
    std::vector<std::string> remote_urls;    ///< https://<public or tailnet name>/shell/ (the remote gate is on)
};

class IDevServerControl {
public:
    virtual ~IDevServerControl() = default;
    /// Serve `source` (a profile path). Another project being served is stopped
    /// first: one project at a time. False with `err` when it cannot start.
    virtual bool            start(const std::string& source, std::string& err) = 0;
    /// Stop serving and close the project (back to the launcher).
    virtual void            stop() = 0;
    virtual DevServerStatus status() const = 0;
    /// Bumped by every start / stop.
    virtual uint64_t        revision() const = 0;
};

} // namespace ergo::studio::project
