#pragma once
/// One accepted TCP connection on one gate, from the first byte to the answer
/// (or to the WebSocket it becomes). Loop thread only, except the handler,
/// which runs on the pool and posts its answer back.
///   head   portus::parse_request_head within max_header_bytes (400 / 431)
///   body   Content-Length only (chunked: 411), at most max_body_bytes (413)
///   then   RequestPipeline::answer on the pool, or the WebSocket handshake
#include "server_core.h"

#include <portus/http_message.hpp>
#include <portus/tcp_conn.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace ergo::studio::web {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Clock = std::chrono::steady_clock;

    /// `forget` drops the listener's reference (called once, when the socket closes
    /// or the connection becomes a WebSocket).
    Connection(ServerCore& core, net::Gate gate, std::shared_ptr<portus::TcpConn> tcp, std::function<void()> forget);

    void start();
    /// Past its deadline (still receiving, or lingering after the answer).
    bool expired(Clock::time_point now) const { return now >= deadline_; }
    void close();

private:
    enum class State { Head, Body, Handling, Closing, Upgraded };

    void drop();   // run forget_ once
    void on_data(const char* data, std::size_t len);
    void on_head();
    void dispatch(std::string body);
    void upgrade(std::string rest);
    void answer(const std::string& bytes);

    ServerCore&                      core_;
    net::Gate                        gate_;
    std::shared_ptr<portus::TcpConn> tcp_;
    std::function<void()>            forget_;
    std::string                      in_;
    portus::RequestHead              head_;
    std::size_t                      body_len_ = 0;
    State                            state_    = State::Head;
    Clock::time_point                deadline_;
};

} // namespace ergo::studio::web
