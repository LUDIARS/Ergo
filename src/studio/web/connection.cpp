#include "connection.h"

#include "head_request.h"
#include "wire.h"

#include <portus/ws.hpp>

#include <cstdint>
#include <exception>
#include <utility>

namespace ergo::studio::web {
namespace {

constexpr int kLingerMs = 2000;   // after the answer, how long the peer may take to close

} // namespace

Connection::Connection(ServerCore& core, net::Gate gate, std::shared_ptr<portus::TcpConn> tcp,
                       std::function<void()> forget)
    : core_(core), gate_(gate), tcp_(std::move(tcp)), forget_(std::move(forget)),
      deadline_(Clock::now() + std::chrono::milliseconds(core.limits.request_timeout_ms)) {}

void Connection::start() {
    std::weak_ptr<Connection> weak = shared_from_this();
    tcp_->set_on_data([weak](const char* data, std::size_t len) {
        if (auto self = weak.lock()) self->on_data(data, len);
    });
    tcp_->set_on_close([weak] {
        if (auto self = weak.lock()) self->drop();
    });
    tcp_->start();
}

void Connection::close() {
    if (tcp_) tcp_->close();
}

void Connection::drop() {
    if (!forget_) return;
    auto forget = std::move(forget_);
    forget_     = nullptr;
    forget();
}

void Connection::on_data(const char* data, std::size_t len) {
    if (state_ != State::Head && state_ != State::Body) return;
    const auto keep = shared_from_this();
    in_.append(data, len);
    if (state_ == State::Head) {
        std::size_t               consumed = 0;
        portus::RequestHead       head;
        const portus::ParseResult r = portus::parse_request_head(in_, core_.limits.max_header_bytes, head, consumed);
        if (r == portus::ParseResult::Incomplete) return;
        if (r == portus::ParseResult::Bad) {
            const auto end       = in_.find("\r\n\r\n");
            const bool too_large = end == std::string::npos || end + 4 > core_.limits.max_header_bytes;
            core_.log.warn("refused a request head", portus::Json::object({{"status", too_large ? 431 : 400}}));
            answer(wire_refusal(too_large ? 431 : 400, too_large ? "header too large" : "bad request"));
            return;
        }
        head_ = std::move(head);
        in_.erase(0, consumed);
        on_head();
        if (state_ != State::Body) return;   // answered, or now a WebSocket
    }
    if (state_ == State::Body && in_.size() >= body_len_) {
        std::string body = in_.substr(0, body_len_);
        in_.clear();
        dispatch(std::move(body));
    }
}

void Connection::on_head() {
    if (wants_websocket(head_)) {
        std::string rest;
        rest.swap(in_);   // frames that came with the handshake go to the WebSocket
        upgrade(std::move(rest));
        return;
    }
    // Content-Length only: a chunked body could be framed differently by a proxy in front.
    if (head_.headers.find("transfer-encoding")) {
        answer(wire_refusal(411, "length required"));
        return;
    }
    std::uint64_t length = 0;
    if (!portus::parse_content_length(head_.headers, length)) {
        answer(wire_refusal(400, "bad Content-Length"));
        return;
    }
    if (length > core_.limits.max_body_bytes) {
        core_.log.warn("refused a body over the limit",
                       portus::Json::object({{"bytes", static_cast<double>(length)},
                                             {"limit", static_cast<double>(core_.limits.max_body_bytes)}}));
        answer(wire_refusal(413, "payload too large"));
        return;
    }
    body_len_ = static_cast<std::size_t>(length);
    state_    = State::Body;
    if (length > 0 && portus::header_has_token(head_.headers.get("expect"), "100-continue")) {
        tcp_->write("HTTP/1.1 100 Continue\r\n\r\n");
    }
}

void Connection::dispatch(std::string body) {
    state_    = State::Handling;
    deadline_ = Clock::time_point::max();   // a handler may take long (an AI call); the sweep leaves it alone
    tcp_->pause_read(true);
    auto req = to_request(head_, std::move(body), gate_);
    if (!req) {
        answer(wire_refusal(400, "bad request target"));
        return;
    }
    const bool  head_only = req->method == "HEAD";
    auto        self      = shared_from_this();
    auto        shared    = std::make_shared<net::HttpRequest>(std::move(*req));
    ServerCore* core      = &core_;
    const bool  queued    = core_.pool->submit([self, core, shared, head_only] {
        std::string bytes;
        try {
            bytes = wire_response(core->pipeline.answer(*shared), head_only);
        } catch (...) {
            bytes = wire_refusal(500, "internal error");
        }
        if (core->closed.load()) return;
        auto out = std::make_shared<std::string>(std::move(bytes));
        core->loop.post([self, out] { self->answer(*out); });
    });
    if (!queued) answer(wire_refusal(503, "server busy"));
}

void Connection::upgrade(std::string rest) {
    auto req = to_request(head_, {}, gate_);
    if (!req) {
        answer(wire_refusal(400, "bad request target"));
        return;
    }
    if (const auto bad = portus::validate_upgrade(head_.method, head_.headers)) {
        answer(wire_refusal(400, *bad));
        return;
    }
    std::string plugin_id;
    if (const auto refusal = core_.pipeline.admit_ws(*req, plugin_id)) {
        answer(wire_refusal(refusal->status, refusal->reason));
        return;
    }
    tcp_->write(portus::build_accept_response(head_.headers.get("sec-websocket-key")));
    if (tcp_->closed()) return;

    state_ = State::Upgraded;
    core_.reap_sessions();
    auto ws = std::make_shared<portus::WebSocket>(core_.loop, false, core_.limits.max_ws_payload);
    ws->attach(tcp_);   // the WebSocket takes the socket's callbacks from here on
    auto conn    = std::make_shared<PortusWsConnection>(core_.next_ws_id++, ws);
    auto session = std::make_unique<WsSession>();
    session->bind(ws, conn, core_.pipeline.ws(), plugin_id, *req, core_.plain_log);
    core_.sessions.push_back(std::move(session));
    core_.sockets.push_back(ws);

    const auto keep = shared_from_this();
    drop();
    tcp_.reset();
    ws->receive(rest);   // frames that came with the handshake
}

void Connection::answer(const std::string& bytes) {
    if (!tcp_ || tcp_->closed()) return;
    state_    = State::Closing;
    deadline_ = Clock::now() + std::chrono::milliseconds(kLingerMs);
    tcp_->write(bytes);
    if (tcp_) tcp_->end();
}

} // namespace ergo::studio::web
