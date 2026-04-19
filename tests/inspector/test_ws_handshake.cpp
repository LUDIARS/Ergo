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
