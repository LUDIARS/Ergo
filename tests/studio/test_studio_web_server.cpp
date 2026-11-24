// The Studio's web server on Portus (spec/feature/tool/ergo.md "Web サーバ", "口と入口"):
// the route table (anything else is 404), the body limit, the two gates (the remote one off by
// default, loopback-only operations refused there whatever the Host), and the engine's
// WebSocket client on the same port (the wire contract of the old net::Server). Proxy header
// values are made in the test; the server looks at their presence only.
#include "ergo/common/ws_client.h"
#include "ergo/studio/ingress/entrance.h"
#include "ergo/studio/web/request_pipeline.h"
#include "ergo/studio/web/route_table.h"
#include "ergo/studio/web/web_server.h"

#include <portus/socket.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <random>
#include <string>
#include <vector>

using namespace ergo::studio;

namespace {

std::string made_up(const char* what) {
    static std::mt19937_64 gen{std::random_device{}()};
    return std::string(what) + "-" + std::to_string(gen());
}

const char* kTailnet = "devbox.tail1234.ts.net";

class EchoHttp final : public net::IHttpHandler {
public:
    net::HttpResponse handle(const net::HttpRequest& req) override {
        ++calls;
        last_who = req.who;
        return net::HttpResponse::json(200, "{\"path\":\"" + req.path + "\"}");
    }
    std::atomic<int> calls{0};
    std::string      last_who;
};

class EchoWs final : public net::IWsAcceptor {
public:
    bool accepts(const std::string& id) const override { return id == "variable"; }
    net::WsHandlers accept(const std::string&, const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) override {
        net::WsHandlers h;
        h.on_text = [conn](const std::string& t) { conn->send_text("echo:" + t); };
        return h;
    }
};

void quiet(const std::string&, const std::string&) {}

net::HttpRequest request(const std::string& method, const std::string& path, const std::string& host,
                         net::Gate gate = net::Gate::local) {
    net::HttpRequest r;
    r.method          = method;
    r.path            = path;
    r.target          = path;
    r.gate            = gate;
    r.headers["host"] = host;
    return r;
}

struct Pipeline {
    EchoHttp              http;
    EchoWs                ws;
    std::vector<int>      logged;
    web::RequestPipeline  pipeline{ingress::EntrancePolicy{"127.0.0.1", {"studio.example.com"}, {kTailnet}},
                                  web::RouteTable({"variable", "studio", "assets"}), http, ws,
                                  [this](const net::HttpRequest&, int status, const std::string&) { logged.push_back(status); }};
};

/// One raw HTTP exchange with 127.0.0.1:<port>; the whole answer (the server closes after it).
std::string round_trip(uint16_t port, const std::string& wire) {
    const portus::socket_t fd = portus::connect_tcp("127.0.0.1", port, 5000);
    std::size_t sent = 0;
    while (sent < wire.size()) {
        const long n = portus::socket_send(fd, wire.data() + sent, wire.size() - sent);
        if (n > 0) sent += static_cast<std::size_t>(n);
        else if (n < 0 && portus::last_error_would_block()) portus::wait_socket(fd, portus::kWrite, 5000);
        else break;
    }
    std::string out;
    char        buf[4096];
    for (;;) {
        if (portus::wait_socket(fd, portus::kRead, 5000) == 0) break;
        const long n = portus::socket_recv(fd, buf, sizeof buf);
        if (n > 0) out.append(buf, static_cast<std::size_t>(n));
        else if (n < 0 && portus::last_error_would_block()) continue;
        else break;
    }
    portus::close_socket(fd);
    return out;
}

int status_of(const std::string& answer) {
    return answer.size() >= 12 ? std::stoi(answer.substr(9, 3)) : 0;
}

std::string get(const std::string& path, const std::string& host, const std::string& extra = {}) {
    return "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n" + extra + "\r\n";
}

std::string post(const std::string& path, const std::string& host, const std::string& body, const std::string& extra = {}) {
    return "POST " + path + " HTTP/1.1\r\nHost: " + host + "\r\nContent-Type: application/json\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\n" + extra + "\r\n" + body;
}

web::WebServerConfig loopback_config() {
    web::WebServerConfig c;
    c.local_port    = 0;
    c.public_hosts  = {"studio.example.com"};
    c.tailnet_hosts = {kTailnet};
    return c;
}

} // namespace

// ---- the pipeline (no sockets) ----------------------------------------------------------

TEST(StudioWebRoutes, OnlyTheTableReachesAHandler) {
    Pipeline p;
    for (const char* path : {"/api/health", "/api/plugins", "/api/launcher/projects", "/shell/", "/shell/tela.js",
                             "/variable/api/vars", "/assets"}) {
        auto r = request("GET", path, "127.0.0.1:5170");
        EXPECT_EQ(p.pipeline.answer(r).status, 200);
    }
    for (const auto& [method, path] : std::vector<std::pair<std::string, std::string>>{
             {"GET", "/"}, {"GET", "/nothing"}, {"GET", "/api/other"}, {"POST", "/api/health"},
             {"GET", "/shell/other.js"}, {"GET", "/variablex/api"}, {"DELETE", "/api/launcher/add"}}) {
        auto r = request(method, path, "127.0.0.1:5170");
        EXPECT_EQ(p.pipeline.answer(r).status, 404);
    }
    EXPECT_EQ(p.http.calls.load(), 7);
}

TEST(StudioWebRoutes, TheRemoteGateRefusesLoopbackOnlyOperations) {
    Pipeline p;
    const std::string login = made_up("someone");
    auto remote = [&](const char* method, const char* path) {
        auto r = request(method, path, kTailnet, net::Gate::remote);
        r.headers[ingress::kTailscaleLoginHeader] = login;
        return p.pipeline.answer(r).status;
    };
    EXPECT_EQ(remote("POST", "/api/launcher/add"), 403);
    EXPECT_EQ(remote("POST", "/api/launcher/remove"), 403);
    EXPECT_EQ(remote("POST", "/studio/api/plugins/install"), 403);
    EXPECT_EQ(p.http.calls.load(), 0);
    EXPECT_EQ(remote("GET", "/studio/api/plugins"), 200);   // reading stays open
    EXPECT_EQ(remote("POST", "/api/launcher/open"), 200);
    EXPECT_EQ(p.http.last_who, login);   // who is recorded

    auto local = request("POST", "/api/launcher/add", "127.0.0.1:5170");
    EXPECT_EQ(p.pipeline.answer(local).status, 200);   // the local gate does everything
}

TEST(StudioWebRoutes, TheEntranceComesBeforeTheTable) {
    Pipeline p;
    auto no_access = request("GET", "/api/health", "studio.example.com", net::Gate::remote);
    EXPECT_EQ(p.pipeline.answer(no_access).status, 403);
    auto no_login = request("GET", "/api/health", kTailnet, net::Gate::remote);
    EXPECT_EQ(p.pipeline.answer(no_login).status, 403);
    auto unknown = request("GET", "/nothing", "other.example.com", net::Gate::remote);
    EXPECT_EQ(p.pipeline.answer(unknown).status, 403);
    auto published_on_local = request("GET", "/api/health", "studio.example.com");
    published_on_local.headers[ingress::kAccessAssertionHeader] = made_up("assertion");
    EXPECT_EQ(p.pipeline.answer(published_on_local).status, 403);
    auto cross = request("POST", "/variable/api/set", "127.0.0.1:5170");
    cross.headers["origin"] = "https://evil.example";
    EXPECT_EQ(p.pipeline.answer(cross).status, 403);
    EXPECT_EQ(p.http.calls.load(), 0);
    EXPECT_EQ(p.logged.size(), 5u);   // every refusal is recorded
}

TEST(StudioWebRoutes, WebSocketsNeedTheEntranceAndAnAcceptor) {
    Pipeline    p;
    std::string id;
    auto        ok = request("GET", "/variable/ws", "127.0.0.1:5170");
    EXPECT_FALSE(p.pipeline.admit_ws(ok, id).has_value());
    EXPECT_EQ(id, "variable");
    auto none = request("GET", "/assets/ws", "127.0.0.1:5170");
    ASSERT_TRUE(p.pipeline.admit_ws(none, id).has_value());
    EXPECT_EQ(p.pipeline.admit_ws(none, id)->status, 404);
    auto remote = request("GET", "/variable/ws", kTailnet, net::Gate::remote);
    ASSERT_TRUE(p.pipeline.admit_ws(remote, id).has_value());   // no login: refused, Origin or not
    EXPECT_EQ(p.pipeline.admit_ws(remote, id)->status, 403);
}

// ---- the server (loopback sockets) ------------------------------------------------------

TEST(StudioWebServer, TheRemoteGateIsOffByDefault) {
    EchoHttp        http;
    EchoWs          ws;
    web::WebServer  server(loopback_config(), http, ws, {"variable"}, &quiet);
    std::string     err;
    ASSERT_TRUE(server.start(err));
    EXPECT_NE(server.local_port(), 0);
    EXPECT_FALSE(server.remote_port().has_value());
    EXPECT_FALSE(web::WebServerConfig{}.remote_port.has_value());
    server.stop();
}

TEST(StudioWebServer, AnswersTheTableAnd404sTheRest) {
    EchoHttp       http;
    EchoWs         ws;
    web::WebServer server(loopback_config(), http, ws, {"variable"}, &quiet);
    std::string    err;
    ASSERT_TRUE(server.start(err));
    const std::string host = "127.0.0.1:" + std::to_string(server.local_port());
    const std::string ok   = round_trip(server.local_port(), get("/variable/api/vars", host));
    EXPECT_EQ(status_of(ok), 200);
    EXPECT_NE(ok.find("{\"path\":\"/variable/api/vars\"}"), std::string::npos);
    EXPECT_EQ(status_of(round_trip(server.local_port(), get("/not/on/the/table", host))), 404);
    EXPECT_EQ(status_of(round_trip(server.local_port(), "NOT A REQUEST\r\n\r\n")), 400);
    EXPECT_EQ(http.calls.load(), 1);
    server.stop();
}

TEST(StudioWebServer, ABodyOverTheLimitIsRefusedBeforeTheHandler) {
    EchoHttp             http;
    EchoWs               ws;
    web::WebServerConfig cfg = loopback_config();
    cfg.limits.max_body_bytes = 64;
    web::WebServer server(cfg, http, ws, {"variable"}, &quiet);
    std::string    err;
    ASSERT_TRUE(server.start(err));
    const std::string host = "127.0.0.1:" + std::to_string(server.local_port());
    EXPECT_EQ(status_of(round_trip(server.local_port(), post("/variable/api/set", host, std::string(65, 'x')))), 413);
    EXPECT_EQ(status_of(round_trip(server.local_port(), post("/variable/api/set", host, std::string(64, 'x')))), 200);
    EXPECT_EQ(http.calls.load(), 1);
    server.stop();
}

TEST(StudioWebServer, TheGateIsThePortNotAHeader) {
    EchoHttp             http;
    EchoWs               ws;
    web::WebServerConfig cfg = loopback_config();
    cfg.remote_port          = 0;   // on, any free port
    web::WebServer server(cfg, http, ws, {"variable"}, &quiet);
    std::string    err;
    ASSERT_TRUE(server.start(err));
    ASSERT_TRUE(server.remote_port().has_value());
    const uint16_t    remote = *server.remote_port();
    const std::string login  = "Tailscale-User-Login: " + made_up("someone") + "\r\n";

    EXPECT_EQ(status_of(round_trip(remote, post("/api/launcher/add", kTailnet, "{}", login))), 403);
    EXPECT_EQ(status_of(round_trip(remote, get("/variable/api/vars", kTailnet, login))), 200);
    EXPECT_EQ(status_of(round_trip(remote, get("/variable/api/vars", kTailnet))), 403);   // no login
    const std::string local_host = "127.0.0.1:" + std::to_string(remote);
    EXPECT_EQ(status_of(round_trip(remote, get("/variable/api/vars", local_host))), 403);   // a loopback name remotely
    EXPECT_EQ(status_of(round_trip(server.local_port(), get("/variable/api/vars", kTailnet, login))), 403);
    server.stop();
}

TEST(StudioWebServer, EngineWsClientConnectsAndEchoes) {
    EchoHttp       http;
    EchoWs         ws;
    web::WebServer server(loopback_config(), http, ws, {"variable"}, &quiet);
    std::string    err;
    ASSERT_TRUE(server.start(err));

    std::mutex              m;
    std::condition_variable cv;
    std::string             got;
    ergo::common::ws::Client client;
    client.set_on_open([&] { client.send_text("hi"); });
    client.set_on_text([&](const std::string& t) {
        std::lock_guard<std::mutex> lk(m);
        got = t;
        cv.notify_all();
    });
    client.start("127.0.0.1", server.local_port(), "/variable/ws");
    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait_for(lk, std::chrono::seconds(5), [&] { return !got.empty(); });
    }
    client.stop();
    server.stop();
    EXPECT_EQ(got, "echo:hi");
}
