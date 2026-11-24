#pragma once
/// The remote dev server's lifecycle (spec/plan/2026-11-23-studio-launcher-and-devserver.md, B):
/// "リモート開発サーバ起動" opens the project in the launcher and starts the web
/// server; "停止" stops it and closes the project; starting another project
/// stops the current one first (one at a time). How the server is made is
/// given from outside (ServeFn), so this class knows neither sockets nor Portus.
/// Thread safe; start / stop must not be called from a request the server is answering.
#include "ergo/studio/project/dev_server_control.h"
#include "ergo/studio/project/launcher.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::project {

/// A running server. Destroying it stops it.
class IServing {
public:
    virtual ~IServing() = default;
    virtual uint16_t                local_port() const  = 0;
    /// The remote gate's port; nullopt when the remote gate is off.
    virtual std::optional<uint16_t> remote_port() const = 0;
};

/// Start a server, or nullptr with `err` (e.g. the port is taken).
using ServeFn = std::function<std::unique_ptr<IServing>(std::string& err)>;

struct DevServerOptions {
    std::string              bind_host = "127.0.0.1";
    std::string              page      = "/";   ///< what the URLs point at (e.g. "/shell/")
    std::vector<std::string> published_names;   ///< public and tailnet names (shown as https://<name><page>)
};

class DevServer final : public IDevServerControl {
public:
    DevServer(Launcher& launcher, ServeFn serve, DevServerOptions options = {});
    ~DevServer() override;
    DevServer(const DevServer&)            = delete;
    DevServer& operator=(const DevServer&) = delete;

    bool            start(const std::string& source, std::string& err) override;
    void            stop() override;
    DevServerStatus status() const override;
    uint64_t        revision() const override { return revision_.load(); }

    /// Serve with no project open (ergo_studio --headless without --project:
    /// the launcher's HTTP API opens one later). True when already serving.
    bool serve_without_project(std::string& err);

private:
    void stop_locked();

    mutable std::mutex        mutex_;
    Launcher&                 launcher_;
    ServeFn                   serve_;
    DevServerOptions          options_;
    std::unique_ptr<IServing> serving_;
    std::atomic<uint64_t>     revision_{0};
};

} // namespace ergo::studio::project
