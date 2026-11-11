#pragma once
/// Loopback HTTP/1.1 + WebSocket server of Ergo Studio. It keeps the wire
/// contract of the old Node server so engine-side clients connect unchanged:
///   HTTP  /api/*, /<plugin>/api/*        (handled by IHttpHandler)
///   WS    /<plugin>/ws                   (handled by IWsAcceptor)
/// Every request passes request_guard first. One thread per connection; the
/// Studio serves a handful of local clients, not the network.
#include "ergo/studio/net/http_message.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ergo::studio::net {

/// One accepted WebSocket. send_text / close are safe from any thread.
class IWsConnection {
public:
    virtual ~IWsConnection() = default;
    virtual uint64_t id() const = 0;
    virtual void     send_text(const std::string& payload) = 0;
    virtual void     close(uint16_t code = 1000) = 0;
    virtual bool     is_open() const = 0;
};

/// Callbacks of one accepted WebSocket. Called on the connection's thread.
struct WsHandlers {
    std::function<void(const std::string&)> on_text;
    std::function<void()>                   on_close;
};

class IHttpHandler {
public:
    virtual ~IHttpHandler() = default;
    /// Answer one request. Called on the connection's thread.
    virtual HttpResponse handle(const HttpRequest& req) = 0;
};

class IWsAcceptor {
public:
    virtual ~IWsAcceptor() = default;
    /// True when a plugin with `plugin_id` takes WebSocket connections.
    virtual bool accepts(const std::string& plugin_id) const = 0;
    /// Bind a newly opened connection to the plugin.
    virtual WsHandlers accept(const std::string& plugin_id, const HttpRequest& req,
                              std::shared_ptr<IWsConnection> conn) = 0;
};

struct ServerConfig {
    std::string bind_host = "127.0.0.1";
    uint16_t    port      = 5170;   // 0 = pick a free port (tests)
    /// Connections served at once (each is a thread holding up to one request
    /// head + body or one WS message, 32 MiB). The Studio's own clients (a
    /// browser, a few engine links per plugin) stay far below; more are
    /// answered 503 and closed.
    std::size_t max_connections = 64;
};

using LogFn = std::function<void(const std::string& level, const std::string& msg)>;

class Server {
public:
    Server(ServerConfig cfg, IHttpHandler& http, IWsAcceptor& ws, LogFn log);
    ~Server();
    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    /// Bind and start accepting. False (with `err`) when the port is taken.
    bool start(std::string& err);
    void stop();
    /// Bound port (the real one when cfg.port was 0).
    uint16_t port() const { return bound_port_; }

private:
    void accept_loop();
    void serve_connection(std::intptr_t sock);

    ServerConfig                cfg_;
    IHttpHandler&               http_;
    IWsAcceptor&                ws_;
    LogFn                       log_;
    std::intptr_t               listen_sock_ = -1;
    uint16_t                    bound_port_  = 0;
    std::atomic<bool>           running_{false};
    std::thread                 accept_thread_;
    std::mutex                  conn_mtx_;
    struct ConnThread {
        std::thread                        thread;
        std::shared_ptr<std::atomic<bool>> done;
    };
    std::vector<ConnThread>     conn_threads_;
    std::vector<std::intptr_t>  open_socks_;
    std::atomic<uint64_t>       next_id_{1};
};

} // namespace ergo::studio::net
