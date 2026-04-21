#include "ergo/inspector/ws_handshake.h"
#include "gtest/gtest.h"

#include <cstdint>
#include <string>

using namespace ergo::inspector::ws;

TEST(WsHandshake, ComputeAcceptRfcExample) {
    // RFC 6455 §1.3 example.
    EXPECT_EQ(compute_accept("dGhlIHNhbXBsZSBub25jZQ=="),
              "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST(WsHandshake, ParseHttpRequest) {
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:17317\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    HttpRequest req;
    ASSERT_TRUE(parse_http_request(raw, req));
    EXPECT_EQ(req.method, "GET");
    EXPECT_EQ(req.path, "/");
    EXPECT_EQ(req.upgrade, "websocket");
    EXPECT_EQ(req.connection, "Upgrade");
    EXPECT_EQ(req.sec_websocket_key, "x3JJHMbDL1EzLkh9GBhXDw==");
    EXPECT_EQ(req.total_size, raw.size());
}

TEST(WsHandshake, ParseHttpReturnsFalseOnIncomplete) {
    HttpRequest req;
    EXPECT_FALSE(parse_http_request("GET / HTTP/1.1\r\nHost: x\r\n", req));
}

TEST(WsHandshake, ParseHttpExtractsOrigin) {
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:17317\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
        "Origin: http://localhost:5170\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    HttpRequest req;
    ASSERT_TRUE(parse_http_request(raw, req));
    EXPECT_EQ(req.origin, "http://localhost:5170");
}

TEST(WsOrigin, AllowsLoopbackAndMissing) {
    EXPECT_TRUE(is_origin_allowed(""));                          // non-browser
    EXPECT_TRUE(is_origin_allowed("null"));                      // file://
    EXPECT_TRUE(is_origin_allowed("http://localhost"));
    EXPECT_TRUE(is_origin_allowed("http://localhost:5170"));
    EXPECT_TRUE(is_origin_allowed("https://127.0.0.1:8443"));
    EXPECT_TRUE(is_origin_allowed("http://[::1]:17317"));
}

TEST(WsOrigin, RejectsRemoteOrigins) {
    EXPECT_FALSE(is_origin_allowed("http://example.com"));
    EXPECT_FALSE(is_origin_allowed("https://evil.test:443"));
    EXPECT_FALSE(is_origin_allowed("http://192.168.0.5"));
    EXPECT_FALSE(is_origin_allowed("not a url"));
}

TEST(WsFrame, EncodeShortText) {
    std::string f = encode_frame(WsOpcode::Text, "hello");
    ASSERT_EQ(f.size(), 7u);
    EXPECT_EQ(static_cast<uint8_t>(f[0]), 0x81);
    EXPECT_EQ(static_cast<uint8_t>(f[1]), 5);
    EXPECT_EQ(f.substr(2), "hello");
}

TEST(WsFrame, DecodeMaskedClientFrame) {
    uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
    std::string raw;
    raw.push_back(static_cast<char>(0x81));
    raw.push_back(static_cast<char>(0x82));
    for (int i = 0; i < 4; ++i) raw.push_back(static_cast<char>(mask[i]));
    raw.push_back(static_cast<char>('h' ^ mask[0]));
    raw.push_back(static_cast<char>('i' ^ mask[1]));

    DecodedFrame fr = decode_frame(raw);
    ASSERT_TRUE(fr.ok);
    EXPECT_EQ(fr.opcode, WsOpcode::Text);
    EXPECT_EQ(fr.payload, "hi");
    EXPECT_EQ(fr.consumed, raw.size());
}

TEST(WsFrame, DecodeIncompleteReturnsNotOkNoError) {
    DecodedFrame fr = decode_frame(std::string("\x81", 1));
    EXPECT_FALSE(fr.ok);
    EXPECT_FALSE(fr.protocol_err);
    EXPECT_EQ(fr.consumed, 0u);
}

TEST(WsFrame, EncodeLongPayloadUses16BitLength) {
    std::string p(200, 'x');
    std::string f = encode_frame(WsOpcode::Text, p);
    EXPECT_GE(f.size(), 4u);
    EXPECT_EQ(static_cast<uint8_t>(f[1]), 126);
    EXPECT_EQ(static_cast<uint8_t>(f[2]), 0);
    EXPECT_EQ(static_cast<uint8_t>(f[3]), 200);
}

TEST(WsFrame, DecodeRejectsOversizedPayload) {
    // Craft a header that claims a 2 GiB payload using the 64-bit length
    // form (opcode 0x81 = FIN|Text, mask bit set, length sentinel 127).
    // Decoding must flag protocol_err without attempting to buffer 2 GiB.
    std::string raw;
    raw.push_back(static_cast<char>(0x81));
    raw.push_back(static_cast<char>(0xFF)); // mask=1, len=127
    uint64_t plen = 1ull << 31;
    for (int i = 7; i >= 0; --i) raw.push_back(static_cast<char>((plen >> (i * 8)) & 0xFF));

    DecodedFrame fr = decode_frame(raw);
    EXPECT_FALSE(fr.ok);
    EXPECT_TRUE(fr.protocol_err);
}

TEST(WsFrame, DecodeRejectsUnmaskedClientFrame) {
    // Unmasked frame from a client must be rejected per RFC 6455 §5.1.
    std::string raw;
    raw.push_back(static_cast<char>(0x81));
    raw.push_back(static_cast<char>(0x02)); // mask=0, len=2
    raw.push_back('h');
    raw.push_back('i');
    DecodedFrame fr = decode_frame(raw);
    EXPECT_FALSE(fr.ok);
    EXPECT_TRUE(fr.protocol_err);
}
