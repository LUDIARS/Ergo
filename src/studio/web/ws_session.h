#pragma once
/// One accepted WebSocket between a portus::WebSocket (event-loop thread) and a
/// plugin's net::WsHandlers. The plugin callbacks may block (they did on the old
/// thread-per-connection server), so they run in order on the session's own
/// thread, never on the event loop: accept first, then every text message, then
/// the close.
#include "ergo/studio/net/server.h"

#include <portus/ws.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace ergo::studio::web {

/// net::IWsConnection over a portus::WebSocket (send / close from any thread).
class PortusWsConnection final : public net::IWsConnection {
public:
    PortusWsConnection(uint64_t id, std::weak_ptr<portus::WebSocket> ws) : id_(id), ws_(std::move(ws)) {}
    uint64_t id() const override { return id_; }
    void     send_text(const std::string& payload) override;
    void     close(uint16_t code) override;
    bool     is_open() const override;
    void     mark_closed() { open_ = false; }

private:
    uint64_t                         id_;
    std::weak_ptr<portus::WebSocket> ws_;
    std::atomic<bool>                open_{true};
};

class WsSession {
public:
    WsSession();
    ~WsSession();   // joins the thread (call after the close was posted)
    WsSession(const WsSession&)            = delete;
    WsSession& operator=(const WsSession&) = delete;

    /// Bind `ws` to the plugin: acceptor.accept() on the session thread, then
    /// messages and the close. Call on the loop thread, before ws->receive().
    void bind(const std::shared_ptr<portus::WebSocket>& ws, std::shared_ptr<PortusWsConnection> conn,
              net::IWsAcceptor& acceptor, const std::string& plugin_id, const net::HttpRequest& req, net::LogFn log);

    /// True once the close ran (the thread is about to end).
    bool finished() const { return finished_.load(); }
    /// Make the thread end after what is queued (the close included). Idempotent.
    void end();

private:
    void post(std::function<void()> task);
    void run();

    std::mutex                        mutex_;
    std::condition_variable           wake_;
    std::deque<std::function<void()>> queue_;
    bool                              ending_ = false;
    std::atomic<bool>                 finished_{false};
    net::WsHandlers                   handlers_;   // session thread only
    std::thread                       thread_;
};

} // namespace ergo::studio::web
