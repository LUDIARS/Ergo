// ergo_studio_platform's HTTP client outside Windows: the request checks
// (pure), and — when the build has libcurl — real loopback requests to the
// Studio's own net::Server and to a tiny in-test listener (chunked bodies).
#include "http_request_check.h"

#include <gtest/gtest.h>

#include <string>

using namespace ergo::studio::platform;
using namespace ergo::studio::platform::detail;

namespace {

HttpClientRequest request(std::string url, std::string method = "GET") {
    HttpClientRequest r;
    r.url    = std::move(url);
    r.method = std::move(method);
    return r;
}

bool has_line(const CheckedRequest& c, const std::string& line) {
    for (const auto& l : c.header_lines) {
        if (l == line) return true;
    }
    return false;
}

} // namespace

TEST(StudioPlatformHttpCheck, AcceptsHttpAndHttpsUrls) {
    for (const char* url : {"http://127.0.0.1:4410/v1/tool/state", "https://lesson.example.org/v1/tool/pair",
                            "HTTPS://example.org", "http://[::1]:8080/x?y=1", "http://host"}) {
        const RequestCheck c = check_http_request(request(url));
        EXPECT_TRUE(c.request.has_value());
        EXPECT_TRUE(c.error.empty());
    }
    EXPECT_TRUE(check_http_request(request("https://a.example")).request->https);
    EXPECT_FALSE(check_http_request(request("http://a.example")).request->https);
}

TEST(StudioPlatformHttpCheck, RefusesOtherSchemesAndBrokenUrlsWithoutEchoingThem) {
    for (const char* url : {"ftp://example.org/file", "file:///etc/passwd", "ws://127.0.0.1/x", "example.org",
                            "http://", "https:///path", "http://:80/", "http://user:secret@host/",
                            "http://host/a b", "http://host/\r\nX: y", ""}) {
        const RequestCheck c = check_http_request(request(url));
        EXPECT_FALSE(c.request.has_value());
        EXPECT_FALSE(c.error.empty());
        EXPECT_TRUE(c.error.find("secret") == std::string::npos);
        EXPECT_TRUE(c.error.find("etc/passwd") == std::string::npos);
    }
}

TEST(StudioPlatformHttpCheck, MethodsHeadersAndTimeout) {
    EXPECT_TRUE(check_http_request(request("http://h/", "PATCH")).request.has_value());
    EXPECT_TRUE(check_http_request(request("http://h/", "POST")).request->is_post);
    EXPECT_TRUE(check_http_request(request("http://h/", "GET")).request->is_get);
    EXPECT_FALSE(check_http_request(request("http://h/", "get")).request.has_value());
    EXPECT_FALSE(check_http_request(request("http://h/", "GET /x HTTP/1.1\r\n")).request.has_value());
    EXPECT_FALSE(check_http_request(request("http://h/", "")).request.has_value());

    HttpClientRequest r = request("http://h/", "POST");
    r.headers = {{"Authorization", "Bearer abc"}, {"X-Empty", ""}};
    RequestCheck c = check_http_request(r);
    ASSERT_TRUE(c.request.has_value());
    EXPECT_TRUE(has_line(*c.request, "Authorization: Bearer abc"));
    EXPECT_TRUE(has_line(*c.request, "X-Empty;"));         // sent empty, not dropped
    EXPECT_TRUE(has_line(*c.request, "Content-Type:"));    // no form type added by libcurl
    EXPECT_TRUE(has_line(*c.request, "Expect:"));          // no 100-continue round trip

    r.headers = {{"content-type", "application/json"}, {"EXPECT", "100-continue"}};
    c = check_http_request(r);
    ASSERT_TRUE(c.request.has_value());
    EXPECT_EQ(c.request->header_lines.size(), 2u);   // the caller's own, nothing added

    r.headers = {{"X-Bad", "a\r\nInjected: 1"}};
    EXPECT_FALSE(check_http_request(r).request.has_value());
    r.headers = {{"X Bad", "a"}};
    EXPECT_FALSE(check_http_request(r).request.has_value());
    r.headers = {{"", "a"}};
    EXPECT_FALSE(check_http_request(r).request.has_value());
    r.headers.clear();
    r.timeout_ms = 0;
    EXPECT_FALSE(check_http_request(r).request.has_value());
}

TEST(StudioPlatformHttpCheck, ResponseLimit) {
    std::string body;
    EXPECT_TRUE(append_capped(body, "abcd", 4, 6));
    EXPECT_TRUE(append_capped(body, "ef", 2, 6));
    EXPECT_FALSE(append_capped(body, "g", 1, 6));
    EXPECT_EQ(body, std::string("abcdef"));
    std::string empty;
    EXPECT_FALSE(append_capped(empty, "toolong", 7, 6));
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(kMaxResponseBytes, std::size_t{64} * 1024 * 1024);
}

#ifdef ERGO_STUDIO_TEST_CURL

#include "curl_http_client.h"

#include "ergo/studio/net/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

using namespace ergo::studio;

namespace {

/// Loopback traffic must not go to a proxy configured in the environment.
void bypass_proxy_for_loopback() {
    ::setenv("NO_PROXY", "127.0.0.1,localhost", 1);
    ::setenv("no_proxy", "127.0.0.1,localhost", 1);
}

class Handler final : public net::IHttpHandler {
public:
    std::atomic<int> calls{0};
    net::HttpResponse handle(const net::HttpRequest& req) override {
        ++calls;
        if (req.path == "/slow") {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            return net::HttpResponse::json(200, "{}");
        }
        if (req.path == "/missing") return net::HttpResponse::json(404, R"({"error":"link_not_found"})");
        if (req.path == "/big") return net::HttpResponse::text(200, std::string(2000, 'x'));
        if (req.path == "/redirect") return net::HttpResponse::redirect("/echo");
        const auto auth = req.header("authorization");
        const auto ct   = req.header("content-type");
        return net::HttpResponse::text(200, req.method + "|" + req.path + "|" + auth.value_or("-") + "|" +
                                                ct.value_or("-") + "|" + (req.header("expect") ? "expect" : "-") +
                                                "|" + req.body);
    }
};

class NoWs final : public net::IWsAcceptor {
public:
    bool            accepts(const std::string&) const override { return false; }
    net::WsHandlers accept(const std::string&, const net::HttpRequest&, std::shared_ptr<net::IWsConnection>) override {
        return {};
    }
};

struct Loopback {
    Loopback() : server({"127.0.0.1", 0}, handler, ws, [](const std::string&, const std::string&) {}) {
        bypass_proxy_for_loopback();
        std::string err;
        started = server.start(err);
    }
    ~Loopback() { server.stop(); }
    std::string url(const std::string& path) const { return "http://127.0.0.1:" + std::to_string(server.port()) + path; }

    Handler     handler;
    NoWs        ws;
    net::Server server;
    bool        started = false;
};

/// A one-shot listener that answers the first connection with `reply` and closes it.
class RawListener {
public:
    enum class Read { Headers, FirstBytes };   ///< read a whole request head, or only what arrives first (a TLS hello)
    explicit RawListener(std::string reply, Read read = Read::Headers) : reply_(std::move(reply)), read_(read) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family      = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port        = 0;
        ok_ = fd_ >= 0 && ::bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0 && ::listen(fd_, 1) == 0;
        socklen_t len = sizeof(a);
        ok_ = ok_ && ::getsockname(fd_, reinterpret_cast<sockaddr*>(&a), &len) == 0;
        port_ = ntohs(a.sin_port);
        if (ok_) thread_ = std::thread([this] { serve(); });
    }
    ~RawListener() {
        join();
        if (fd_ >= 0) ::close(fd_);
    }
    void join() {
        if (thread_.joinable()) thread_.join();
    }
    bool               ok() const { return ok_; }
    const std::string& received() const { return got_; }   ///< after join()
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_) + "/"; }

private:
    void serve() {
        const int c = ::accept(fd_, nullptr, nullptr);
        if (c < 0) return;
        char buf[1024];
        while (got_.find("\r\n\r\n") == std::string::npos) {
            const ssize_t n = ::recv(c, buf, sizeof(buf), 0);
            if (n <= 0) break;
            got_.append(buf, static_cast<std::size_t>(n));
            if (read_ == Read::FirstBytes) break;
        }
        std::size_t sent = 0;
        while (sent < reply_.size()) {
            const ssize_t n = ::send(c, reply_.data() + sent, reply_.size() - sent, MSG_NOSIGNAL);
            if (n <= 0) break;
            sent += static_cast<std::size_t>(n);
        }
        ::close(c);
    }

    std::string reply_;
    Read        read_;
    std::string got_;
    int         fd_   = -1;
    uint16_t    port_ = 0;
    bool        ok_   = false;
    std::thread thread_;
};

} // namespace

TEST(StudioPlatformHttpLoopback, GetPostAndOtherMethodsReachTheServerAsSent) {
    Loopback lb;
    ASSERT_TRUE(lb.started);
    auto http = make_system_http_client();

    HttpClientRequest get = request(lb.url("/echo"));
    get.headers           = {{"Authorization", "Bearer 0123"}};
    HttpClientResponse r  = http->send(get);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.body, std::string("GET|/echo|Bearer 0123|-|-|"));

    HttpClientRequest post = request(lb.url("/echo"), "POST");
    post.headers           = {{"Content-Type", "application/json"}};
    post.body              = R"({"link":"7K2M9QX4TB","tool":"ergo-studio"})" + std::string(2048, ' ');   // > 1 KiB: no Expect
    r                      = http->send(post);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(r.body, "POST|/echo|-|application/json|-|" + post.body);

    HttpClientRequest bare = request(lb.url("/echo"), "POST");
    bare.body              = "x=1";
    EXPECT_EQ(http->send(bare).body, std::string("POST|/echo|-|-|-|x=1"));   // libcurl added no form Content-Type

    HttpClientRequest put = request(lb.url("/echo"), "PUT");
    put.body              = "v";
    EXPECT_EQ(http->send(put).body, std::string("PUT|/echo|-|-|-|v"));
    EXPECT_EQ(http->send(request(lb.url("/echo"), "DELETE")).body, std::string("DELETE|/echo|-|-|-|"));

    r = http->send(request(lb.url("/redirect")));   // followed, like WinHTTP
    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.body, std::string("GET|/echo|-|-|-|"));
}

TEST(StudioPlatformHttpLoopback, ErrorStatusesAreResponsesNotTransportErrors) {
    Loopback lb;
    ASSERT_TRUE(lb.started);
    const HttpClientResponse r = make_system_http_client()->send(request(lb.url("/missing")));
    EXPECT_TRUE(r.transport_ok);
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.status, 404);
    EXPECT_EQ(r.body, std::string(R"({"error":"link_not_found"})"));
}

TEST(StudioPlatformHttpLoopback, TimeoutRefusalLimitAndNoTlsDowngrade) {
    Loopback lb;
    ASSERT_TRUE(lb.started);
    auto http = make_system_http_client();

    HttpClientRequest slow = request(lb.url("/slow"));
    slow.timeout_ms        = 200;
    const auto t0          = std::chrono::steady_clock::now();
    HttpClientResponse r   = http->send(slow);
    EXPECT_FALSE(r.transport_ok);
    EXPECT_TRUE(r.timed_out);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::milliseconds(1400));

    r = make_curl_http_client(1000)->send(request(lb.url("/big")));   // 2000 bytes against a 1000-byte limit
    EXPECT_FALSE(r.transport_ok);
    EXPECT_FALSE(r.timed_out);
    EXPECT_TRUE(r.error.find("larger than 1000 bytes") != std::string::npos);
    EXPECT_TRUE(r.body.empty());

    const int before = lb.handler.calls.load();
    r = http->send(request("ftp://127.0.0.1:" + std::to_string(lb.server.port()) + "/echo"));
    EXPECT_FALSE(r.transport_ok);
    EXPECT_TRUE(r.error.find("only http:// and https://") != std::string::npos);
    EXPECT_EQ(lb.handler.calls.load(), before);

    lb.server.stop();   // nobody listens any more
    r = http->send(request(lb.url("/echo")));
    EXPECT_FALSE(r.transport_ok);
    EXPECT_FALSE(r.timed_out);
    EXPECT_FALSE(r.error.empty());
}

TEST(StudioPlatformHttpLoopback, HttpsToAPlainServerFailsAsTlsAndIsNeverDowngraded) {
    bypass_proxy_for_loopback();
    RawListener plain("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok", RawListener::Read::FirstBytes);
    ASSERT_TRUE(plain.ok());
    std::string https = plain.url();
    https.replace(0, 4, "https");
    HttpClientRequest req = request(https);
    req.timeout_ms        = 5000;
    const HttpClientResponse r = make_system_http_client()->send(req);
    EXPECT_FALSE(r.transport_ok);
    EXPECT_FALSE(r.timed_out);
    EXPECT_EQ(r.status, 0);
    EXPECT_TRUE(r.body.empty());
    EXPECT_TRUE(r.error.find("TLS failed") != std::string::npos);
    EXPECT_TRUE(r.error.find("127.0.0.1") == std::string::npos);   // generic text, no host or port
    plain.join();
    ASSERT_FALSE(plain.received().empty());
    EXPECT_EQ(static_cast<unsigned char>(plain.received()[0]), 0x16u);   // a TLS handshake record, not "GET /"
}

TEST(StudioPlatformHttpLoopback, ChunkedAndCloseDelimitedBodies) {
    bypass_proxy_for_loopback();
    {
        RawListener chunked("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
                            "5\r\nhello\r\n7\r\n, world\r\n0\r\n\r\n");
        ASSERT_TRUE(chunked.ok());
        const HttpClientResponse r = make_system_http_client()->send(request(chunked.url()));
        EXPECT_TRUE(r.ok());
        EXPECT_EQ(r.body, std::string("hello, world"));
    }
    {
        RawListener close_delimited("HTTP/1.1 201 Created\r\nConnection: close\r\n\r\nuntil close");
        ASSERT_TRUE(close_delimited.ok());
        const HttpClientResponse r = make_system_http_client()->send(request(close_delimited.url()));
        EXPECT_EQ(r.status, 201);
        EXPECT_EQ(r.body, std::string("until close"));
    }
    {
        RawListener broken("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\nnot hex\r\n");
        ASSERT_TRUE(broken.ok());
        const HttpClientResponse r = make_system_http_client()->send(request(broken.url()));
        EXPECT_FALSE(r.transport_ok);   // a malformed chunked body is an error, not a short body
    }
}

#endif // ERGO_STUDIO_TEST_CURL
