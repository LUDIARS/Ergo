#include "ergo/studio/net/ws_frame.h"

namespace ergo::studio::net {

std::string encode_ws_frame(WsOpcode op, const std::string& payload) {
    std::string out;
    out.push_back(static_cast<char>(0x80 | static_cast<uint8_t>(op)));
    const uint64_t n = payload.size();
    if (n < 126) {
        out.push_back(static_cast<char>(n));
    } else if (n <= 0xFFFF) {
        out.push_back(static_cast<char>(126));
        out.push_back(static_cast<char>((n >> 8) & 0xFF));
        out.push_back(static_cast<char>(n & 0xFF));
    } else {
        out.push_back(static_cast<char>(127));
        for (int i = 7; i >= 0; --i) out.push_back(static_cast<char>((n >> (i * 8)) & 0xFF));
    }
    out += payload;
    return out;
}

std::string encode_ws_close(uint16_t code, const std::string& reason) {
    std::string p;
    p.push_back(static_cast<char>(code >> 8));
    p.push_back(static_cast<char>(code & 0xFF));
    p += reason.substr(0, 123);
    return encode_ws_frame(WsOpcode::Close, p);
}

WsDecodeResult WsFrameDecoder::next(WsMessage& out) {
    for (;;) {
        if (buf_.size() < 2) return WsDecodeResult::NeedMore;
        const auto b0 = static_cast<uint8_t>(buf_[0]);
        const auto b1 = static_cast<uint8_t>(buf_[1]);
        const bool fin    = (b0 & 0x80) != 0;
        const uint8_t rsv = b0 & 0x70;
        const auto op     = static_cast<WsOpcode>(b0 & 0x0F);
        const bool masked = (b1 & 0x80) != 0;
        if (rsv != 0 || !masked) return WsDecodeResult::ProtocolError;

        uint64_t len = b1 & 0x7F;
        std::size_t off = 2;
        if (len == 126) {
            if (buf_.size() < 4) return WsDecodeResult::NeedMore;
            len = (uint64_t(uint8_t(buf_[2])) << 8) | uint8_t(buf_[3]);
            off = 4;
        } else if (len == 127) {
            if (buf_.size() < 10) return WsDecodeResult::NeedMore;
            len = 0;
            for (int i = 0; i < 8; ++i) len = (len << 8) | uint8_t(buf_[2 + i]);
            off = 10;
        }
        const bool control = (static_cast<uint8_t>(op) & 0x08) != 0;
        if (control && (len > 125 || !fin)) return WsDecodeResult::ProtocolError;
        if (len > max_payload_ || (!control && frag_payload_.size() + len > max_payload_)) return WsDecodeResult::TooBig;
        if (buf_.size() < off + 4 + len) return WsDecodeResult::NeedMore;

        const char* mask = buf_.data() + off;
        std::string payload = buf_.substr(off + 4, static_cast<std::size_t>(len));
        for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
        buf_.erase(0, off + 4 + static_cast<std::size_t>(len));

        if (control) {
            out.opcode  = op;
            out.payload = std::move(payload);
            return WsDecodeResult::Message;
        }
        if (op == WsOpcode::Continuation) {
            if (!in_fragment_) return WsDecodeResult::ProtocolError;
            frag_payload_ += payload;
        } else if (op == WsOpcode::Text || op == WsOpcode::Binary) {
            if (in_fragment_) return WsDecodeResult::ProtocolError;
            frag_opcode_  = op;
            frag_payload_ = std::move(payload);
            in_fragment_  = true;
        } else {
            return WsDecodeResult::ProtocolError;
        }
        if (fin) {
            in_fragment_ = false;
            out.opcode   = frag_opcode_;
            out.payload  = std::move(frag_payload_);
            frag_payload_.clear();
            return WsDecodeResult::Message;
        }
    }
}

} // namespace ergo::studio::net
