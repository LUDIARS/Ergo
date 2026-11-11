#include "ergo/studio/net/server.h"

#include "ergo/studio/net/codec.h"
#include "ergo/studio/net/http_parser.h"
#include "ergo/studio/net/request_guard.h"
#include "ergo/studio/net/ws_frame.h"
#include "socket.h"

#include <algorithm>
#include <cctype>

namespace ergo::studio::net {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool header_has_token(const HttpRequest& req, const std::string& name, const std::string& token) {
    const auto v = req.header(name);
    return v && lower(*v).find(token) != std::string::npos;
}

/// Plugin id of "/<id>/ws" or "/<id>/ws/", empty otherwise.
std::string ws_plugin_id(const std::string& path) {
    if (path.size() < 5 || path[0] != '/') return {};
    const auto slash = path.find('/', 1);
    if (slash == std::string::npos || slash == 1) return {};
    const std::string rest = path.substr(slash);
    if (rest != "/ws" && rest != "/ws/") return {};
    return path.substr(1, slash - 1);
}

std::string json_error(const std::string& msg) {
    std::string esc;
    for (char c : msg) {
        if (c == '"' || c == '\\') esc.push_back('\\');
        if (static_cast<unsigned char>(c) < 0x20) continue;
        esc.push_back(c);
    }
    return "{\"ok\":false,\"err\":\"" + esc + "\"}";
}

class WsConnection final : public IWsConnection {
public:
    WsConnection(uint64_t id, sock::Handle s) : id_(id), sock_(s) {}
    uint64_t id() const override { return id_; }
    bool is_open() const override { return open_.load(); }
    void send_text(const std::string& payload) override { send_raw(encode_ws_frame(WsOpcode::Text, payload)); }
    void close(uint16_t code) override {
        if (!open_.exchange(false)) return;
        std::lock_guard<std::mutex> lk(mtx_);
        const std::string f = encode_ws_close(code);
        sock::send_all(sock_, f.data(), f.size());
        sock::shutdown_both(sock_);
    }
    void send_raw(const std::string& frame) {
        if (!open_.load()) return;
        std::lock_guard<std::mutex> lk(mtx_);
        if (!sock::send_all(sock_, frame.data(), frame.size())) open_ = false;
    }
    void mark_closed() { open_ = false; }

private:
    uint64_t          id_;
    sock::Handle      sock_;
    std::atomic<bool> open_{true};
    std::mutex        mtx_;
};

} // namespace

Server::Server(ServerConfig cfg, IHttpHandler& http, IWsAcceptor& ws, LogFn log)
    : cfg_(std::move(cfg)), http_(http), ws_(ws), log_(std::move(log)) {}

Server::~Server() { stop(); }

bool Server::start(std::string& err) {
    if (running_) return true;
    listen_sock_ = sock::listen_tcp(cfg_.bind_host, cfg_.port, bound_port_, err);
    if (listen_sock_ == sock::kInvalid) return false;
    running_ = true;
    accept_thread_ = std::thread([this] { accept_loop(); });
    return true;
}

void Server::stop() {
    if (!running_.exchange(false)) return;
    sock::shutdown_both(listen_sock_);   // wakes accept_one() in the accept thread
    sock::close(listen_sock_);
    if (accept_thread_.joinable()) accept_thread_.join();
    listen_sock_ = sock::kInvalid;       // only after the join: accept_loop() reads it
    std::vector<ConnThread> threads;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        for (auto s : open_socks_) sock::shutdown_both(s);
        threads.swap(conn_threads_);
    }
    for (auto& t : threads) if (t.thread.joinable()) t.thread.join();
}

void Server::accept_loop() {
    while (running_) {
        const auto c = sock::accept_one(listen_sock_);
        if (c == sock::kInvalid) {
            if (!running_) break;
            continue;
        }
        std::lock_guard<std::mutex> lk(conn_mtx_);
        // Reap finished connections so their threads do not pile up.
        for (auto it = conn_threads_.begin(); it != conn_threads_.end();) {
            if (it->done->load()) {
                if (it->thread.joinable()) it->thread.join();
                it = conn_threads_.erase(it);
            } else {
                ++it;
            }
        }
        open_socks_.push_back(c);
        auto done = std::make_shared<std::atomic<bool>>(false);
        conn_threads_.push_back({std::thread([this, c, done] { serve_connection(c); done->store(true); }), done});
    }
}

void Server::serve_connection(std::intptr_t s) {
    HttpRequestParser parser;
    char buf[16 * 1024];
    ParseState st = ParseState::NeedMore;
    while (st == ParseState::NeedMore) {
        const long n = sock::recv_some(s, buf, sizeof(buf));
        if (n <= 0) break;
        st = parser.feed(buf, static_cast<std::size_t>(n));
    }

    auto finish = [&] {
        sock::close(s);
        std::lock_guard<std::mutex> lk(conn_mtx_);
        open_socks_.erase(std::remove(open_socks_.begin(), open_socks_.end(), s), open_socks_.end());
    };

    if (st == ParseState::Error) {
        const std::string out = serialize_response(HttpResponse::json(parser.error_status(), json_error("bad request")));
        sock::send_all(s, out.data(), out.size());
        finish();
        return;
    }
    if (st != ParseState::Complete) { finish(); return; }

    const HttpRequest& req = parser.request();
    const auto host   = req.header("host");
    const auto origin = req.header("origin");

    const bool upgrade = req.method == "GET" && header_has_token(req, "upgrade", "websocket")
                      && header_has_token(req, "connection", "upgrade");
    if (!upgrade) {
        HttpResponse res;
        if (const auto problem = cross_site_problem(host, origin, cfg_.bind_host)) {
            res = HttpResponse::json(403, json_error(*problem));
        } else {
            try {
                res = http_.handle(req);
            } catch (const std::exception& e) {
                log_("error", std::string("[ergo] handler threw on ") + req.path + ": " + e.what());
                res = HttpResponse::json(500, json_error("internal error"));
            }
        }
        const std::string out = serialize_response(res);
        sock::send_all(s, out.data(), out.size());
        finish();
        return;
    }

    const std::string plugin_id = ws_plugin_id(req.path);
    const auto key = req.header("sec-websocket-key");
    const auto ver = req.header("sec-websocket-version");
    if (plugin_id.empty() || !ws_.accepts(plugin_id) || !key || !ver || *ver != "13") { finish(); return; }
    if (const auto problem = ws_handshake_problem(host, origin, cfg_.bind_host)) {
        log_("warn", "[ergo] refused WS " + req.path + ": " + *problem);
        const std::string out = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
        sock::send_all(s, out.data(), out.size());
        finish();
        return;
    }

    const std::string hs = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: " + websocket_accept_key(*key) + "\r\n\r\n";
    if (!sock::send_all(s, hs.data(), hs.size())) { finish(); return; }

    auto conn = std::make_shared<WsConnection>(next_id_++, s);
    WsHandlers handlers;
    try {
        handlers = ws_.accept(plugin_id, req, conn);
    } catch (const std::exception& e) {
        log_("error", "[ergo] plugin '" + plugin_id + "' WS accept threw: " + e.what());
        conn->close(1011);
        finish();
        return;
    }

    WsFrameDecoder dec;
    const std::string leftover = parser.take_leftover();
    if (!leftover.empty()) dec.append(leftover.data(), leftover.size());
    bool alive = true;
    while (alive) {
        WsMessage msg;
        const auto r = dec.next(msg);
        if (r == WsDecodeResult::NeedMore) {
            const long n = sock::recv_some(s, buf, sizeof(buf));
            if (n <= 0) break;
            dec.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        if (r == WsDecodeResult::TooBig)        { conn->close(1009); break; }
        if (r == WsDecodeResult::ProtocolError) { conn->close(1002); break; }
        switch (msg.opcode) {
            case WsOpcode::Text:
                if (handlers.on_text) {
                    try { handlers.on_text(msg.payload); }
                    catch (const std::exception& e) {
                        log_("error", "[ergo] plugin '" + plugin_id + "' WS handler threw: " + e.what());
                    }
                }
                break;
            case WsOpcode::Ping:  conn->send_raw(encode_ws_frame(WsOpcode::Pong, msg.payload)); break;
            case WsOpcode::Close: conn->close(1000); alive = false; break;
            default: break;  // binary and pong are ignored, as in the TS server
        }
    }
    conn->mark_closed();
    if (handlers.on_close) {
        try { handlers.on_close(); } catch (...) {}
    }
    finish();
}

} // namespace ergo::studio::net
