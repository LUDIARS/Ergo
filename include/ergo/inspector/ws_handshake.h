#pragma once

/// WebSocket helpers — RFC 6455 handshake + frame encode/decode.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ergo::inspector::ws {

std::string compute_accept(const std::string& sec_websocket_key);

struct HttpRequest {
    std::string method;
    std::string path;
    std::string sec_websocket_key;
    std::string upgrade;
    std::string connection;
    std::size_t total_size = 0;
};
bool parse_http_request(const std::string& buf, HttpRequest& out);

std::string build_handshake_response(const std::string& accept);
std::string build_http_html_response(const std::string& body);

enum class WsOpcode : uint8_t {
    Continuation = 0x0,
    Text         = 0x1,
    Binary       = 0x2,
    Close        = 0x8,
    Ping         = 0x9,
    Pong         = 0xA,
};

struct DecodedFrame {
    bool        ok            = false;
    bool        protocol_err  = false;
    std::size_t consumed      = 0;
    bool        fin           = true;
    WsOpcode    opcode        = WsOpcode::Text;
    std::string payload;
};

DecodedFrame decode_frame(const std::string& buf);
std::string  encode_frame(WsOpcode opcode, const std::string& payload);

} // namespace ergo::inspector::ws
