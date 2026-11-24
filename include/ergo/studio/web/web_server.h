#pragma once
/// Ergo Studio's web server on Portus C++ (spec/feature/tool/ergo.md "Web サーバ").
///
/// One Portus event loop serves two listeners:
///   local   127.0.0.1:<PORT>                     this machine (every operation)
///   remote  127.0.0.1:<ERGO_STUDIO_REMOTE_PORT>  the target of cloudflared /
///           `tailscale serve`; off unless a port is given
/// HTTP and WebSocket share each port (/<plugin>/ws, the engine clients' contract).
/// Each request goes through RequestPipeline (entrance, route table, remote
/// scope, handler); handlers run on a Portus thread pool, WebSocket callbacks on
/// their session's thread, never on the event loop. One request per connection.
#include "ergo/studio/ingress/entrance.h"
#include "ergo/studio/net/server.h"
#include "ergo/studio/project/dev_server.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::web {

struct WebLimits {
    std::size_t max_header_bytes   = 64 * 1024;          ///< request line + headers, else 431
    std::size_t max_body_bytes     = 32 * 1024 * 1024;   ///< Content-Length above it: 413
    std::size_t max_ws_payload     = 32 * 1024 * 1024;   ///< a bigger WebSocket message: close 1009
    std::size_t max_connections    = 256;                ///< per listener; more are closed at once
    std::size_t worker_threads     = 8;                  ///< handlers (they may block)
    std::size_t max_pending        = 512;                ///< handlers waiting for a worker, else 503
    int         request_timeout_ms = 30000;              ///< to receive a whole request
};

struct WebServerConfig {
    std::string             bind_host  = "127.0.0.1";
    uint16_t                local_port = 5170;   ///< 0 = any free port (tests)
    std::optional<uint16_t> remote_port;         ///< nullopt = the remote gate is off (default); 0 = any free port
    /// Public and tailnet names (bind_host is taken from above).
    std::vector<std::string> public_hosts;
    std::vector<std::string> tailnet_hosts;
    WebLimits               limits;
};

class WebServer final : public project::IServing {
public:
    /// `plugin_ids` = the mounted plugins (their /<id> subtree is on the route table).
    WebServer(WebServerConfig cfg, net::IHttpHandler& http, net::IWsAcceptor& ws, std::vector<std::string> plugin_ids,
              net::LogFn log);
    ~WebServer() override;
    WebServer(const WebServer&)            = delete;
    WebServer& operator=(const WebServer&) = delete;

    /// Listen on both gates. False with `err` (nothing left listening) when a port is taken.
    bool start(std::string& err);
    /// Close the listeners and every connection; waits for running handlers.
    void stop();

    uint16_t                local_port() const override { return local_port_; }
    std::optional<uint16_t> remote_port() const override { return remote_port_; }

private:
    struct Impl;
    std::unique_ptr<Impl>   impl_;
    uint16_t                local_port_ = 0;
    std::optional<uint16_t> remote_port_;
};

} // namespace ergo::studio::web
