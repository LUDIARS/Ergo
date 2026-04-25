// ergo_custos モジュールの起動/停止 + handler 動作の smoke test。
//
// 実 HTTP リクエストはテスト内で raw socket を叩いて確認する (依存無し)。
// ポートは OS 任せ (host=127.0.0.1, port=0) にして bind 競合を避ける。

#include "gtest/gtest.h"
#include "ergo/custos/custos_module.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   using sock_t = SOCKET;
   static int  close_sock(sock_t s) { return closesocket(s); }
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using sock_t = int;
   static int  close_sock(sock_t s) { return ::close(s); }
#endif

namespace {

#if defined(_WIN32)
struct WsaInit {
    WsaInit() { WSADATA d{}; WSAStartup(MAKEWORD(2,2), &d); }
    ~WsaInit() { WSACleanup(); }
};
static WsaInit g_wsa;
#endif

/// 単純な HTTP 1.0 リクエストを送って応答全体を返す。
std::string http_request(uint16_t port, const std::string& req) {
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return {};
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
#if defined(_WIN32)
    InetPtonA(AF_INET, "127.0.0.1", &addr.sin_addr);
#else
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
#endif
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_sock(s);
        return {};
    }
    int sent = 0;
    while (sent < int(req.size())) {
        int n = send(s, req.data() + sent, int(req.size() - sent), 0);
        if (n <= 0) break;
        sent += n;
    }
    std::string out;
    char buf[4096];
    while (true) {
        int n = recv(s, buf, int(sizeof(buf)), 0);
        if (n <= 0) break;
        out.append(buf, std::size_t(n));
    }
    close_sock(s);
    return out;
}

uint16_t start_and_get_port() {
    ergo::custos::StartConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;        // OS 任せ
    if (!ergo::custos::start(cfg)) return 0;
    return ergo::custos::bound_port();
}

} // anonymous

TEST(CustosModule, health_endpoint_returns_ok) {
    auto port = start_and_get_port();
    EXPECT_NE(port, 0);

    auto resp = http_request(port,
        "GET /health HTTP/1.0\r\nHost: localhost\r\n\r\n");

    EXPECT_NE(resp.find("HTTP/1.0 200"), std::string::npos);
    EXPECT_NE(resp.find("\r\nok"), std::string::npos);

    ergo::custos::shutdown();
}

TEST(CustosModule, key_endpoint_calls_handler) {
    auto port = start_and_get_port();

    std::atomic<int>  got_code { -1 };
    std::atomic<bool> got_down { false };
    ergo::custos::set_key_handler([&](int c, bool d) {
        got_code = c;
        got_down = d;
    });

    std::string body = R"({"code": 87, "down": true})";
    std::string req  = "POST /key HTTP/1.0\r\nHost: x\r\nContent-Length: "
                     + std::to_string(body.size())
                     + "\r\n\r\n" + body;
    auto resp = http_request(port, req);

    EXPECT_NE(resp.find("HTTP/1.0 204"), std::string::npos);
    EXPECT_EQ(got_code.load(), 87);
    EXPECT_TRUE(got_down.load());

    ergo::custos::shutdown();
}

TEST(CustosModule, screenshot_endpoint_returns_png_when_provider_ok) {
    auto port = start_and_get_port();

    ergo::custos::set_screenshot_provider([](ergo::custos::ScreenshotData& out) {
        out.width  = 2;
        out.height = 2;
        out.rgba.assign({
            255, 0, 0, 255,    0, 255, 0, 255,
              0, 0, 255, 255,  255, 255, 0, 255,
        });
        return true;
    });

    auto resp = http_request(port,
        "GET /screenshot HTTP/1.0\r\nHost: x\r\n\r\n");

    EXPECT_NE(resp.find("HTTP/1.0 200"),       std::string::npos);
    EXPECT_NE(resp.find("Content-Type: image/png"), std::string::npos);
    // PNG signature 89 50 4E 47 が body 内のどこかにある
    EXPECT_NE(resp.find(std::string("\x89PNG", 4)), std::string::npos);

    ergo::custos::shutdown();
}

TEST(CustosModule, screenshot_returns_503_when_provider_returns_false) {
    auto port = start_and_get_port();
    ergo::custos::set_screenshot_provider([](ergo::custos::ScreenshotData&) {
        return false;
    });
    auto resp = http_request(port, "GET /screenshot HTTP/1.0\r\nHost: x\r\n\r\n");
    EXPECT_NE(resp.find("HTTP/1.0 503"), std::string::npos);
    ergo::custos::shutdown();
}

TEST(CustosModule, unknown_path_404) {
    auto port = start_and_get_port();
    auto resp = http_request(port, "GET /nope HTTP/1.0\r\nHost: x\r\n\r\n");
    EXPECT_NE(resp.find("HTTP/1.0 404"), std::string::npos);
    ergo::custos::shutdown();
}
