// Boundary tests of the Studio server's input limits (http_parser.h,
// ws_frame.h, ServerConfig::max_connections): exactly at the limit, one over,
// malformed input and huge declared sizes.
#include "ergo/studio/net/http_parser.h"
#include "ergo/studio/net/server.h"
#include "ergo/studio/net/ws_frame.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
using raw_socket = SOCKET;
static void close_raw(raw_socket s) { closesocket(s); }
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using raw_socket = int;
static void close_raw(raw_socket s) { ::close(s); }
#endif

using namespace ergo::studio::net;

namespace {

/// "GET / HTTP/1.1\r\nX-Pad: aaa...\r\n\r\n" whose "\r\n\r\n" starts at `end`.
std::string head_ending_at(std::size_t end) {
    std::string h = "GET / HTTP/1.1\r\nX-Pad: ";
    h.append(end - h.size(), 'a');
    return h + "\r\n\r\n";
}

ParseState feed_all(HttpRequestParser& p, const std::string& s) { return p.feed(s.data(), s.size()); }

std::string masked_frame(uint8_t first_byte, uint64_t declared_len, const std::string& payload) {
    std::string f;
    f.push_back(static_cast<char>(first_byte));
    if (declared_len < 126) {
        f.push_back(static_cast<char>(0x80 | declared_len));
    } else if (declared_len <= 0xFFFF) {
        f.push_back(static_cast<char>(0x80 | 126));
        f.push_back(static_cast<char>(declared_len >> 8));
        f.push_back(static_cast<char>(declared_len & 0xFF));
    } else {
        f.push_back(static_cast<char>(0x80 | 127));
        for (int i = 7; i >= 0; --i) f.push_back(static_cast<char>((declared_len >> (i * 8)) & 0xFF));
    }
    const char mask[4] = {0x01, 0x02, 0x03, 0x04};
    f.append(mask, 4);
    for (std::size_t i = 0; i < payload.size(); ++i) f.push_back(static_cast<char>(payload[i] ^ mask[i % 4]));
    return f;
}

WsDecodeResult decode_one(WsFrameDecoder& d, const std::string& bytes, WsMessage& m) {
    d.append(bytes.data(), bytes.size());
    return d.next(m);
}

} // namespace

TEST(StudioNetLimits, HeadExactlyAtLimitAndOneOver) {
    const HttpLimits limits{256, 16, 8};
    HttpRequestParser at(limits);
    EXPECT_EQ(feed_all(at, head_ending_at(256)), ParseState::Complete);
    HttpRequestParser over(limits);
    EXPECT_EQ(feed_all(over, head_ending_at(257)), ParseState::Error);
    EXPECT_EQ(over.error_status(), 431);
    // Without a terminator the head is refused once it passes the limit.
    HttpRequestParser open_ended(limits);
    EXPECT_EQ(feed_all(open_ended, std::string("GET / HTTP/1.1\r\n") + std::string(241, 'a')), ParseState::Error);
    EXPECT_EQ(open_ended.error_status(), 431);
}

TEST(StudioNetLimits, FieldCountExactlyAtLimitAndOneOver) {
    const HttpLimits limits{64 * 1024, 16, 4};
    // 2 query parameters + 2 header lines = 4 fields.
    HttpRequestParser at(limits);
    EXPECT_EQ(feed_all(at, "GET /?a=1&b=2 HTTP/1.1\r\nHost: x\r\nX-A: 1\r\n\r\n"), ParseState::Complete);
    HttpRequestParser over(limits);
    EXPECT_EQ(feed_all(over, "GET /?a=1&b=2 HTTP/1.1\r\nHost: x\r\nX-A: 1\r\nX-B: 2\r\n\r\n"), ParseState::Error);
    EXPECT_EQ(over.error_status(), 431);
    HttpRequestParser many_params(limits);
    EXPECT_EQ(feed_all(many_params, "GET /?a&b&c&d&e HTTP/1.1\r\n\r\n"), ParseState::Error);
    EXPECT_EQ(many_params.error_status(), 431);
}

TEST(StudioNetLimits, ContentLengthAtLimitOneOverAndHuge) {
    const HttpLimits limits{1024, 4};
    HttpRequestParser at(limits);
    EXPECT_EQ(feed_all(at, "POST / HTTP/1.1\r\nContent-Length: 4\r\n\r\nabcd"), ParseState::Complete);
    EXPECT_EQ(at.request().body, "abcd");
    HttpRequestParser over(limits);
    EXPECT_EQ(feed_all(over, "POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\n"), ParseState::Error);
    EXPECT_EQ(over.error_status(), 413);
    for (const std::string huge : {"18446744073709551615", "18446744073709551616", "99999999999999999999999999"}) {
        HttpRequestParser p;
        EXPECT_EQ(feed_all(p, "POST / HTTP/1.1\r\nContent-Length: " + huge + "\r\n\r\n"), ParseState::Error);
        EXPECT_EQ(p.error_status(), 413);
    }
}

TEST(StudioNetLimits, MalformedContentLengthAndHeads) {
    for (const std::string cl : {"-1", "+1", "1 2", "0x1", "", "1\r\nContent-Length: 2"}) {
        HttpRequestParser p;
        EXPECT_EQ(feed_all(p, "POST / HTTP/1.1\r\nContent-Length: " + cl + "\r\n\r\n"), ParseState::Error);
        EXPECT_EQ(p.error_status(), 400);
    }
    for (const std::string head : {"\r\n\r\n", "GET\r\n\r\n", "GET / HTTP/2\r\n\r\n", "GET x HTTP/1.1\r\n\r\n",
                                   "GET / HTTP/1.1\r\nno-colon\r\n\r\n", "GET /%zz HTTP/1.1\r\n\r\n"}) {
        HttpRequestParser p;
        EXPECT_EQ(feed_all(p, head), ParseState::Error);
        EXPECT_EQ(p.error_status(), 400);
    }
}

TEST(StudioNetLimits, WsPayloadExactlyAtLimitAndOneOver) {
    WsMessage m;
    WsFrameDecoder at(8);
    ASSERT_EQ(decode_one(at, masked_frame(0x81, 8, "12345678"), m), WsDecodeResult::Message);
    EXPECT_EQ(m.payload, "12345678");
    WsFrameDecoder over(8);
    EXPECT_EQ(decode_one(over, masked_frame(0x81, 9, "123456789"), m), WsDecodeResult::TooBig);
    // A declared size is refused before its bytes arrive.
    WsFrameDecoder huge(8);
    EXPECT_EQ(decode_one(huge, masked_frame(0x82, UINT64_MAX, ""), m), WsDecodeResult::TooBig);
    WsFrameDecoder big16(8);
    EXPECT_EQ(decode_one(big16, masked_frame(0x81, 0xFFFF, ""), m), WsDecodeResult::TooBig);
}

TEST(StudioNetLimits, WsFragmentsSumToTheLimit) {
    WsMessage m;
    WsFrameDecoder at(8);
    EXPECT_EQ(decode_one(at, masked_frame(0x01, 4, "1234"), m), WsDecodeResult::NeedMore);
    ASSERT_EQ(decode_one(at, masked_frame(0x80, 4, "5678"), m), WsDecodeResult::Message);
    EXPECT_EQ(m.payload, "12345678");
    WsFrameDecoder over(8);
    EXPECT_EQ(decode_one(over, masked_frame(0x01, 4, "1234"), m), WsDecodeResult::NeedMore);
    EXPECT_EQ(decode_one(over, masked_frame(0x80, 5, "56789"), m), WsDecodeResult::TooBig);
}

TEST(StudioNetLimits, WsMalformedFrames) {
    WsMessage m;
    const std::string bad[] = {
        masked_frame(0x81 | 0x40, 1, "x"),    // RSV1 without an extension
        masked_frame(0x89, 126, std::string(126, 'p')),  // control frame over 125 bytes
        masked_frame(0x09, 1, "p"),           // fragmented control frame
        masked_frame(0x80, 1, "x"),           // continuation without a start
        masked_frame(0x83, 1, "x"),           // reserved opcode
    };
    for (const auto& f : bad) {
        WsFrameDecoder d;
        EXPECT_EQ(decode_one(d, f, m), WsDecodeResult::ProtocolError);
    }
    WsFrameDecoder truncated;
    EXPECT_EQ(decode_one(truncated, masked_frame(0x81, 5, "12345").substr(0, 8), m), WsDecodeResult::NeedMore);
}

namespace {

class OkHttp final : public IHttpHandler {
public:
    HttpResponse handle(const HttpRequest&) override { return HttpResponse::json(200, "{}"); }
};

class NoWs final : public IWsAcceptor {
public:
    bool accepts(const std::string&) const override { return false; }
    WsHandlers accept(const std::string&, const HttpRequest&, std::shared_ptr<IWsConnection>) override { return {}; }
};

raw_socket connect_loopback(uint16_t port) {
    raw_socket s = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_raw(s);
        return static_cast<raw_socket>(-1);
    }
    return s;
}

/// Send `req` on a new connection and read the whole answer.
std::string round_trip(uint16_t port, const std::string& req) {
    const raw_socket s = connect_loopback(port);
    if (s == static_cast<raw_socket>(-1)) return {};
    ::send(s, req.data(), static_cast<int>(req.size()), 0);
    std::string out;
    char buf[1024];
    for (;;) {
        const int n = static_cast<int>(::recv(s, buf, sizeof(buf), 0));
        if (n <= 0) break;
        out.append(buf, static_cast<std::size_t>(n));
    }
    close_raw(s);
    return out;
}

} // namespace

TEST(StudioNetLimits, ConnectionsBeyondTheLimitGet503) {
    OkHttp http;
    NoWs ws;
    ServerConfig cfg;
    cfg.port = 0;
    cfg.max_connections = 2;
    Server server(cfg, http, ws, [](const std::string&, const std::string&) {});
    std::string err;
    ASSERT_TRUE(server.start(err));

    // Two idle connections hold both slots (the accept loop takes them in order).
    const raw_socket a = connect_loopback(server.port());
    const raw_socket b = connect_loopback(server.port());
    ASSERT_NE(a, static_cast<raw_socket>(-1));
    ASSERT_NE(b, static_cast<raw_socket>(-1));
    const std::string get = "GET /api/x HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    EXPECT_NE(round_trip(server.port(), get).find("503"), std::string::npos);

    // Once they close, their slots are reaped and requests are served again.
    close_raw(a);
    close_raw(b);
    bool served = false;
    for (int i = 0; i < 100 && !served; ++i) {
        served = round_trip(server.port(), get).find("200") != std::string::npos;
        if (!served) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(served);
    server.stop();
}
