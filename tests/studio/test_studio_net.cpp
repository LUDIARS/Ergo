#include "ergo/common/ws_client.h"
#include "ergo/studio/net/codec.h"
#include "ergo/studio/net/http_parser.h"
#include "ergo/studio/net/request_guard.h"
#include "ergo/studio/net/server.h"
#include "ergo/studio/net/ws_frame.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace ergo::studio::net;

namespace {

std::string hex(const std::array<uint8_t, 20>& d) {
    static const char* h = "0123456789abcdef";
    std::string s;
    for (auto b : d) { s.push_back(h[b >> 4]); s.push_back(h[b & 15]); }
    return s;
}

/// Masked client frame, as a browser / ergo_common_ws would send it.
std::string client_frame(WsOpcode op, const std::string& payload, bool fin = true) {
    std::string f;
    f.push_back(static_cast<char>((fin ? 0x80 : 0) | static_cast<uint8_t>(op)));
    const uint8_t mask[4] = {0x11, 0x22, 0x33, 0x44};
    if (payload.size() < 126) {
        f.push_back(static_cast<char>(0x80 | payload.size()));
    } else {
        f.push_back(static_cast<char>(0x80 | 126));
        f.push_back(static_cast<char>(payload.size() >> 8));
        f.push_back(static_cast<char>(payload.size() & 0xFF));
    }
    for (auto m : mask) f.push_back(static_cast<char>(m));
    for (std::size_t i = 0; i < payload.size(); ++i) f.push_back(static_cast<char>(payload[i] ^ mask[i % 4]));
    return f;
}

} // namespace

TEST(StudioNetCodec, Sha1KnownVectors) {
    EXPECT_EQ(hex(sha1("")), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    EXPECT_EQ(hex(sha1("abc")), "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(StudioNetCodec, AcceptKeyMatchesRfc6455Example) {
    EXPECT_EQ(websocket_accept_key("dGhlIHNhbXBsZSBub25jZQ=="), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST(StudioNetHttp, ParsesRequestAcrossChunks) {
    HttpRequestParser p;
    const std::string a = "POST /studio/api/x?a=1&b=h%20i HTTP/1.1\r\nHost: 127.0.0.1:5170\r\nContent-";
    const std::string b = "Type: application/json\r\nContent-Length: 7\r\n\r\n{\"k\":1}";
    EXPECT_EQ(p.feed(a.data(), a.size()), ParseState::NeedMore);
    ASSERT_EQ(p.feed(b.data(), b.size()), ParseState::Complete);
    EXPECT_EQ(p.request().method, "POST");
    EXPECT_EQ(p.request().path, "/studio/api/x");
    EXPECT_EQ(p.request().query.at("b"), "h i");
    EXPECT_EQ(p.request().header("content-type").value_or(""), "application/json");
    EXPECT_EQ(p.request().body, "{\"k\":1}");
}

TEST(StudioNetHttp, RejectsOversizedAndChunkedBodies) {
    HttpRequestParser small(HttpLimits{1024, 4});
    const std::string big = "POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    EXPECT_EQ(small.feed(big.data(), big.size()), ParseState::Error);
    EXPECT_EQ(small.error_status(), 413);

    HttpRequestParser chunked;
    const std::string c = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n";
    EXPECT_EQ(chunked.feed(c.data(), c.size()), ParseState::Error);
    EXPECT_EQ(chunked.error_status(), 501);
}

TEST(StudioNetWs, DecodesMaskedAndFragmentedFrames) {
    WsFrameDecoder d;
    const std::string f = client_frame(WsOpcode::Text, "hel", false) + client_frame(WsOpcode::Ping, "p")
                        + client_frame(WsOpcode::Continuation, "lo");
    d.append(f.data(), f.size());
    WsMessage m;
    ASSERT_EQ(d.next(m), WsDecodeResult::Message);
    EXPECT_EQ(m.opcode, WsOpcode::Ping);
    ASSERT_EQ(d.next(m), WsDecodeResult::Message);
    EXPECT_EQ(m.opcode, WsOpcode::Text);
    EXPECT_EQ(m.payload, "hello");
    EXPECT_EQ(d.next(m), WsDecodeResult::NeedMore);
}

TEST(StudioNetWs, RejectsUnmaskedAndOversized) {
    WsFrameDecoder d;
    const std::string unmasked = encode_ws_frame(WsOpcode::Text, "x");
    d.append(unmasked.data(), unmasked.size());
    WsMessage m;
    EXPECT_EQ(d.next(m), WsDecodeResult::ProtocolError);

    WsFrameDecoder small(4);
    const std::string f = client_frame(WsOpcode::Text, "12345");
    small.append(f.data(), f.size());
    EXPECT_EQ(small.next(m), WsDecodeResult::TooBig);
}

TEST(StudioNetGuard, TrustedHostsAndOrigins) {
    EXPECT_TRUE(is_trusted_host(std::string("localhost:5170"), ""));
    EXPECT_TRUE(is_trusted_host(std::string("app.localhost"), ""));
    EXPECT_TRUE(is_trusted_host(std::string("127.0.0.1:5170"), ""));
    EXPECT_TRUE(is_trusted_host(std::string("[::1]:5170"), ""));
    EXPECT_FALSE(is_trusted_host(std::string("evil.example:5170"), ""));
    EXPECT_TRUE(is_trusted_host(std::string("devbox:5170"), "devbox"));
    EXPECT_FALSE(is_trusted_host(std::nullopt, ""));

    EXPECT_FALSE(cross_site_problem(std::string("127.0.0.1:5170"), std::nullopt, "").has_value());
    EXPECT_FALSE(cross_site_problem(std::string("127.0.0.1:5170"), std::string("http://127.0.0.1:5170"), "").has_value());
    EXPECT_TRUE(cross_site_problem(std::string("127.0.0.1:5170"), std::string("https://evil.example"), "").has_value());
    EXPECT_TRUE(cross_site_problem(std::string("127.0.0.1:5170"), std::string("null"), "").has_value());
    // A native WS client (no Origin) may dial any host name.
    EXPECT_FALSE(ws_handshake_problem(std::string("devbox.lan:5170"), std::nullopt, "").has_value());
    EXPECT_TRUE(ws_handshake_problem(std::string("127.0.0.1:5170"), std::string("http://evil.example"), "").has_value());
}

namespace {

class EchoHttp final : public IHttpHandler {
public:
    HttpResponse handle(const HttpRequest& req) override {
        return HttpResponse::json(200, "{\"path\":\"" + req.path + "\"}");
    }
};

class EchoWs final : public IWsAcceptor {
public:
    bool accepts(const std::string& id) const override { return id == "variable"; }
    WsHandlers accept(const std::string&, const HttpRequest&, std::shared_ptr<IWsConnection> conn) override {
        WsHandlers h;
        h.on_text = [conn](const std::string& t) { conn->send_text("echo:" + t); };
        return h;
    }
};

} // namespace

TEST(StudioNetServer, EngineWsClientConnectsAndEchoes) {
    EchoHttp http;
    EchoWs ws;
    Server server({"127.0.0.1", 0}, http, ws, [](const std::string&, const std::string&) {});
    std::string err;
    ASSERT_TRUE(server.start(err));

    std::mutex m;
    std::condition_variable cv;
    std::string got;
    ergo::common::ws::Client client;
    client.set_on_open([&] { client.send_text("hi"); });
    client.set_on_text([&](const std::string& t) {
        std::lock_guard<std::mutex> lk(m);
        got = t;
        cv.notify_all();
    });
    client.start("127.0.0.1", server.port(), "/variable/ws");
    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait_for(lk, std::chrono::seconds(5), [&] { return !got.empty(); });
    }
    client.stop();
    server.stop();
    EXPECT_EQ(got, "echo:hi");
}
