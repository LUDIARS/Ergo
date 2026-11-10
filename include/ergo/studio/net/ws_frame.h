#pragma once
/// Server-side WebSocket framing (RFC 6455 §5). Client frames must be masked;
/// server frames are sent unmasked. Fragmented messages are reassembled.
#include <cstddef>
#include <cstdint>
#include <string>

namespace ergo::studio::net {

enum class WsOpcode : uint8_t { Continuation = 0x0, Text = 0x1, Binary = 0x2, Close = 0x8, Ping = 0x9, Pong = 0xA };

/// Largest message accepted (same as the TS server: covers a full
/// ergo_log_live batch and scene JSON). Larger closes with 1009.
constexpr std::size_t kMaxWsPayload = 32 * 1024 * 1024;

/// Encode one unmasked, final frame.
std::string encode_ws_frame(WsOpcode op, const std::string& payload);

/// Close frame with a status code (and optional reason).
std::string encode_ws_close(uint16_t code, const std::string& reason = {});

struct WsMessage {
    WsOpcode    opcode = WsOpcode::Text;  // Text / Binary / Close / Ping / Pong
    std::string payload;
};

enum class WsDecodeResult { NeedMore, Message, ProtocolError, TooBig };

/// Incremental decoder of client frames.
class WsFrameDecoder {
public:
    explicit WsFrameDecoder(std::size_t max_payload = kMaxWsPayload) : max_payload_(max_payload) {}

    void append(const char* data, std::size_t len) { buf_.append(data, len); }

    /// Decode the next complete message from the buffered bytes. Control
    /// frames are returned as soon as they arrive, even mid-fragmentation.
    WsDecodeResult next(WsMessage& out);

private:
    std::size_t max_payload_;
    std::string buf_;
    bool        in_fragment_ = false;
    WsOpcode    frag_opcode_ = WsOpcode::Text;
    std::string frag_payload_;
};

} // namespace ergo::studio::net
