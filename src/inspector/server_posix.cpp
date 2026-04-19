/// POSIX WebSocket server for the Ergo Inspector.
///
/// Single-threaded accept + I/O loop using poll(2). One TCP listener and a
/// vector of per-connection state machines (HTTP handshake → WebSocket).
/// Wakeup is achieved via a self-pipe so stop_server() can interrupt poll().

#include "ergo/inspector/inspector.h"
#include "ergo/inspector/json_min.h"
#include "ergo/inspector/ws_handshake.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ergo::inspector {

namespace {

constexpr const char* kFallbackHtml =
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>Ergo Inspector</title></head>"
    "<body><h3>Ergo Inspector</h3>"
    "<p>The bundled UI was not found. Connect with a WebSocket client to "
    "<code>ws://&lt;host&gt;:&lt;port&gt;/</code> and send "
    "<code>{\"op\":\"enumerate\"}</code>.</p></body></html>";

enum class ConnState : uint8_t { Http, Open, Closing };

struct Conn {
    int                          fd       = -1;
    ConnState                    state    = ConnState::Http;
    std::string                  rx;
    std::string                  tx;
    std::unordered_set<Handle>   subscribed;
    bool                         subscribed_all = false;
    std::chrono::steady_clock::time_point last_seen{};
};

void set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void close_fd(int& fd) {
    if (fd >= 0) { ::close(fd); fd = -1; }
}

jsonm::JsonValue value_to_json(const Value& v) {
    using namespace jsonm;
    switch (v.kind) {
        case VarKind::Bool:   return JsonValue::make_bool(v.b);
        case VarKind::Int32:
        case VarKind::Int64:  return JsonValue::make_number(static_cast<double>(v.i));
        case VarKind::Float:
        case VarKind::Double: return JsonValue::make_number(v.d);
        case VarKind::String: return JsonValue::make_string(v.s);
        case VarKind::Color: {
            auto a = JsonValue::make_array();
            for (int i = 0; i < 4; ++i) a.push(JsonValue::make_number(v.v[i]));
            return a;
        }
        case VarKind::Vec3: {
            auto a = JsonValue::make_array();
            for (int i = 0; i < 3; ++i) a.push(JsonValue::make_number(v.v[i]));
            return a;
        }
    }
    return JsonValue::make_null();
}

bool json_to_value(const jsonm::JsonValue& jv, VarKind kind, Value& out) {
    switch (kind) {
        case VarKind::Bool:
            if (!jv.is_bool()) return false;
            out = Value::of_bool(jv.b); return true;
        case VarKind::Int32:
            if (!jv.is_number()) return false;
            out = Value::of_int32(static_cast<int32_t>(jv.n)); return true;
        case VarKind::Int64:
            if (!jv.is_number()) return false;
            out = Value::of_int64(static_cast<int64_t>(jv.n)); return true;
        case VarKind::Float:
            if (!jv.is_number()) return false;
            out = Value::of_float(static_cast<float>(jv.n)); return true;
        case VarKind::Double:
            if (!jv.is_number()) return false;
            out = Value::of_double(jv.n); return true;
        case VarKind::String:
            if (!jv.is_string()) return false;
            out = Value::of_string(jv.s); return true;
        case VarKind::Color: {
            if (!jv.is_array() || !jv.a || jv.a->size() < 3) return false;
            float r = static_cast<float>((*jv.a)[0].as_number());
            float g = static_cast<float>((*jv.a)[1].as_number());
            float b = static_cast<float>((*jv.a)[2].as_number());
            float a = (jv.a->size() >= 4) ? static_cast<float>((*jv.a)[3].as_number()) : 1.0f;
            out = Value::of_color(r, g, b, a); return true;
        }
        case VarKind::Vec3: {
            if (!jv.is_array() || !jv.a || jv.a->size() < 3) return false;
            out = Value::of_vec3(static_cast<float>((*jv.a)[0].as_number()),
                                 static_cast<float>((*jv.a)[1].as_number()),
                                 static_cast<float>((*jv.a)[2].as_number()));
            return true;
        }
    }
    return false;
}

jsonm::JsonValue meta_to_json(const VarMeta& m) {
    using namespace jsonm;
    auto j = JsonValue::make_object();
    j.set("min",       JsonValue::make_number(m.min));
    j.set("max",       JsonValue::make_number(m.max));
    j.set("step",      JsonValue::make_number(m.step));
    j.set("read_only", JsonValue::make_bool(m.read_only));
    if (!m.category.empty()) j.set("category", JsonValue::make_string(m.category));
    if (!m.unit.empty())     j.set("unit",     JsonValue::make_string(m.unit));
    return j;
}

void enqueue_text(Conn& c, const std::string& payload) {
    c.tx.append(ws::encode_frame(ws::WsOpcode::Text, payload));
}

void enqueue_close(Conn& c) {
    c.tx.append(ws::encode_frame(ws::WsOpcode::Close, ""));
    c.state = ConnState::Closing;
}

bool send_all(Conn& c) {
    while (!c.tx.empty()) {
        ssize_t n = ::send(c.fd, c.tx.data(), c.tx.size(), 0);
        if (n > 0) c.tx.erase(0, static_cast<std::size_t>(n));
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        else return false;
    }
    return true;
}

jsonm::JsonValue build_var_object(const Tweakable& tw, const Value& v) {
    using namespace jsonm;
    auto j = JsonValue::make_object();
    j.set("name",  JsonValue::make_string(tw.name));
    j.set("kind",  JsonValue::make_string(to_string(tw.kind)));
    j.set("value", value_to_json(v));
    j.set("meta",  meta_to_json(tw.meta));
    return j;
}

void handle_message(Conn& c, Inspector& I, const std::string& text) {
    using namespace jsonm;
    JsonValue req;
    if (!parse(text, req) || !req.is_object()) {
        JsonValue e = JsonValue::make_object();
        e.set("ok", JsonValue::make_bool(false));
        e.set("err", JsonValue::make_string("json"));
        enqueue_text(c, serialize(e));
        return;
    }
    const std::string op = req.find("op") ? req.find("op")->as_string() : std::string{};

    if (op == "enumerate") {
        auto snap = I.snapshot();
        JsonValue resp = JsonValue::make_object();
        auto vars = JsonValue::make_array();
        for (auto& tw : snap) {
            if (!tw.getter) continue;
            Value v = tw.getter();
            vars.push(build_var_object(tw, v));
        }
        resp.set("vars", vars);
        enqueue_text(c, serialize(resp));
        return;
    }

    if (op == "get") {
        const std::string name = req.find("name") ? req.find("name")->as_string() : "";
        Handle h = I.find_by_name(name);
        Value  v;
        JsonValue resp = JsonValue::make_object();
        if (h == INVALID_HANDLE || !I.read_value(h, v)) {
            resp.set("ok",  JsonValue::make_bool(false));
            resp.set("err", JsonValue::make_string("unknown"));
        } else {
            resp.set("name",  JsonValue::make_string(name));
            resp.set("value", value_to_json(v));
        }
        enqueue_text(c, serialize(resp));
        return;
    }

    if (op == "set") {
        const std::string name = req.find("name") ? req.find("name")->as_string() : "";
        Handle h = I.find_by_name(name);
        JsonValue resp = JsonValue::make_object();
        if (h == INVALID_HANDLE) {
            resp.set("ok",  JsonValue::make_bool(false));
            resp.set("err", JsonValue::make_string("unknown"));
            enqueue_text(c, serialize(resp));
            return;
        }
        Value cur;
        if (!I.read_value(h, cur)) {
            resp.set("ok",  JsonValue::make_bool(false));
            resp.set("err", JsonValue::make_string("read"));
            enqueue_text(c, serialize(resp));
            return;
        }
        const JsonValue* jv = req.find("value");
        Value v;
        if (!jv || !json_to_value(*jv, cur.kind, v)) {
            resp.set("ok",  JsonValue::make_bool(false));
            resp.set("err", JsonValue::make_string("type"));
            enqueue_text(c, serialize(resp));
            return;
        }
        I.enqueue_write(h, v);
        resp.set("ok", JsonValue::make_bool(true));
        enqueue_text(c, serialize(resp));
        return;
    }

    if (op == "subscribe" || op == "unsubscribe") {
        const std::string name = req.find("name") ? req.find("name")->as_string() : "";
        if (name == "*" || name.empty()) {
            c.subscribed_all = (op == "subscribe");
        } else {
            Handle h = I.find_by_name(name);
            if (h != INVALID_HANDLE) {
                if (op == "subscribe") c.subscribed.insert(h);
                else                    c.subscribed.erase(h);
            }
        }
        JsonValue resp = JsonValue::make_object();
        resp.set("ok", JsonValue::make_bool(true));
        enqueue_text(c, serialize(resp));
        return;
    }

    JsonValue e = JsonValue::make_object();
    e.set("ok",  JsonValue::make_bool(false));
    e.set("err", JsonValue::make_string("op"));
    enqueue_text(c, serialize(e));
}

bool process_http(Conn& c) {
    ws::HttpRequest req;
    if (!ws::parse_http_request(c.rx, req)) return true;

    if (req.upgrade.find("websocket") != std::string::npos &&
        !req.sec_websocket_key.empty()) {
        const std::string accept = ws::compute_accept(req.sec_websocket_key);
        c.tx += ws::build_handshake_response(accept);
        c.state = ConnState::Open;
    } else {
        std::string body;
        FILE* f = std::fopen("inspector_web/index.html", "rb");
        if (!f) f = std::fopen("../inspector_web/index.html", "rb");
        if (!f) f = std::fopen("../../inspector_web/index.html", "rb");
        if (f) {
            std::fseek(f, 0, SEEK_END);
            long sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            body.resize(static_cast<std::size_t>(sz));
            std::fread(body.data(), 1, body.size(), f);
            std::fclose(f);
        } else {
            body = kFallbackHtml;
        }
        c.tx += ws::build_http_html_response(body);
        c.state = ConnState::Closing;
    }
    c.rx.erase(0, req.total_size);
    return true;
}

bool process_ws(Conn& c, Inspector& I) {
    while (!c.rx.empty()) {
        ws::DecodedFrame fr = ws::decode_frame(c.rx);
        if (!fr.ok) {
            if (fr.protocol_err) return false;
            return true;
        }
        c.rx.erase(0, fr.consumed);
        switch (fr.opcode) {
            case ws::WsOpcode::Text:
                handle_message(c, I, fr.payload);
                break;
            case ws::WsOpcode::Ping:
                c.tx.append(ws::encode_frame(ws::WsOpcode::Pong, fr.payload));
                break;
            case ws::WsOpcode::Pong:
                break;
            case ws::WsOpcode::Close:
                enqueue_close(c);
                return true;
            case ws::WsOpcode::Binary:
            case ws::WsOpcode::Continuation:
                break;
        }
    }
    return true;
}

} // namespace

struct Inspector::ServerState {
    std::atomic<bool>                              running   {false};
    std::atomic<bool>                              stop_flag {false};
    std::thread                                    th;
    int                                            listen_fd = -1;
    int                                            wake_r    = -1;
    int                                            wake_w    = -1;
    uint16_t                                       port      = 0;

    std::mutex                                     seen_mtx;
    std::unordered_map<Handle, Value>              last_seen;
};

bool Inspector::start_server(uint16_t port) {
    if (server_) return server_->running.load();
    server_ = new ServerState();
    server_->port = port;

    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::fprintf(stderr, "[ergo::inspector] socket() failed: %s\n", std::strerror(errno));
        delete server_; server_ = nullptr; return false;
    }
    int yes = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    set_nonblock(listen_fd);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(port);
    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(listen_fd, 16) < 0) {
        std::fprintf(stderr, "[ergo::inspector] bind/listen %u failed: %s\n", port, std::strerror(errno));
        ::close(listen_fd);
        delete server_; server_ = nullptr; return false;
    }
    server_->listen_fd = listen_fd;

    int pipefd[2] = {-1, -1};
    if (::pipe(pipefd) < 0) {
        std::fprintf(stderr, "[ergo::inspector] pipe failed: %s\n", std::strerror(errno));
        ::close(listen_fd);
        delete server_; server_ = nullptr; return false;
    }
    set_nonblock(pipefd[0]);
    set_nonblock(pipefd[1]);
    server_->wake_r = pipefd[0];
    server_->wake_w = pipefd[1];

    server_->stop_flag.store(false);
    server_->running.store(true);

    auto* state = server_;
    Inspector* self = this;
    server_->th = std::thread([state, self]{
        std::vector<Conn> conns;
        const auto kPingEvery = std::chrono::seconds(30);
        auto last_ping = std::chrono::steady_clock::now();

        while (!state->stop_flag.load()) {
            std::vector<pollfd> fds;
            fds.reserve(conns.size() + 2);
            fds.push_back({state->listen_fd, POLLIN, 0});
            fds.push_back({state->wake_r,    POLLIN, 0});
            for (auto& c : conns) {
                short events = POLLIN;
                if (!c.tx.empty()) events |= POLLOUT;
                fds.push_back({c.fd, events, 0});
            }

            int rv = ::poll(fds.data(), fds.size(), 200);
            if (rv < 0) {
                if (errno == EINTR) continue;
                std::fprintf(stderr, "[ergo::inspector] poll error: %s\n", std::strerror(errno));
                break;
            }

            if (fds[1].revents & POLLIN) {
                char buf[64];
                while (::read(state->wake_r, buf, sizeof(buf)) > 0) {}
            }

            if (fds[0].revents & POLLIN) {
                while (true) {
                    int cfd = ::accept(state->listen_fd, nullptr, nullptr);
                    if (cfd < 0) break;
                    set_nonblock(cfd);
                    int one = 1;
                    ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                    Conn c;
                    c.fd = cfd;
                    c.last_seen = std::chrono::steady_clock::now();
                    conns.push_back(std::move(c));
                }
            }

            for (std::size_t i = 0; i < conns.size(); ) {
                auto& c = conns[i];
                short revs = fds[i + 2].revents;
                bool drop = false;

                if (revs & (POLLERR | POLLHUP | POLLNVAL)) drop = true;
                if (!drop && (revs & POLLIN)) {
                    char buf[4096];
                    while (true) {
                        ssize_t n = ::recv(c.fd, buf, sizeof(buf), 0);
                        if (n > 0) {
                            c.rx.append(buf, static_cast<std::size_t>(n));
                            c.last_seen = std::chrono::steady_clock::now();
                        } else if (n == 0) { drop = true; break; }
                        else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        else { drop = true; break; }
                    }
                }
                if (!drop) {
                    if (c.state == ConnState::Http) process_http(c);
                    if (c.state == ConnState::Open) {
                        if (!process_ws(c, *self)) drop = true;
                    }
                }
                if (!drop && (revs & POLLOUT || !c.tx.empty())) {
                    if (!send_all(c)) drop = true;
                }
                if (!drop && c.state == ConnState::Closing && c.tx.empty()) drop = true;

                if (drop) {
                    close_fd(c.fd);
                    conns.erase(conns.begin() + i);
                } else {
                    ++i;
                }
            }

            auto now = std::chrono::steady_clock::now();
            if (now - last_ping > kPingEvery) {
                for (auto& c : conns) {
                    if (c.state == ConnState::Open) {
                        c.tx.append(ws::encode_frame(ws::WsOpcode::Ping, ""));
                    }
                }
                last_ping = now;
            }

            // Detect external value changes and broadcast to subscribers.
            auto snap = self->snapshot();
            std::vector<std::pair<std::string, Value>> changes;
            {
                std::lock_guard<std::mutex> lk(state->seen_mtx);
                for (auto& tw : snap) {
                    if (!tw.getter) continue;
                    Handle h = self->find_by_name(tw.name);
                    if (h == INVALID_HANDLE) continue;
                    Value v = tw.getter();
                    auto it = state->last_seen.find(h);
                    if (it == state->last_seen.end()) {
                        state->last_seen.emplace(h, v);
                        continue;
                    }
                    if (!it->second.equals(v)) {
                        it->second = v;
                        changes.emplace_back(tw.name, std::move(v));
                    }
                }
            }
            if (!changes.empty()) {
                for (auto& c : conns) {
                    if (c.state != ConnState::Open) continue;
                    for (auto& [name, v] : changes) {
                        Handle h = self->find_by_name(name);
                        if (!c.subscribed_all && c.subscribed.find(h) == c.subscribed.end()) continue;
                        jsonm::JsonValue m = jsonm::JsonValue::make_object();
                        m.set("op",    jsonm::JsonValue::make_string("changed"));
                        m.set("name",  jsonm::JsonValue::make_string(name));
                        m.set("value", value_to_json(v));
                        enqueue_text(c, jsonm::serialize(m));
                    }
                }
            }
        }

        for (auto& c : conns) close_fd(c.fd);
        close_fd(state->listen_fd);
        close_fd(state->wake_r);
        close_fd(state->wake_w);
        state->running.store(false);
    });

    return true;
}

void Inspector::stop_server() {
    if (!server_) return;
    server_->stop_flag.store(true);
    if (server_->wake_w >= 0) { char x = 1; ::write(server_->wake_w, &x, 1); }
    if (server_->th.joinable()) server_->th.join();
    delete server_; server_ = nullptr;
}

bool Inspector::server_running() const {
    return server_ && server_->running.load();
}

} // namespace ergo::inspector
