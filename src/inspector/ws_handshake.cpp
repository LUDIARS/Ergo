#include "ergo/inspector/ws_handshake.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

namespace ergo::inspector::ws {

namespace {

struct Sha1 {
    uint32_t state[5];
    uint64_t length_bits = 0;
    uint8_t  buf[64];
    std::size_t buf_len = 0;

    Sha1() {
        state[0] = 0x67452301; state[1] = 0xEFCDAB89; state[2] = 0x98BADCFE;
        state[3] = 0x10325476; state[4] = 0xC3D2E1F0;
    }

    static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

    void process_block(const uint8_t* block) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(block[i*4]) << 24) | (uint32_t(block[i*4+1]) << 16) |
                   (uint32_t(block[i*4+2]) << 8) | uint32_t(block[i*4+3]);
        }
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a=state[0], b=state[1], c=state[2], d=state[3], e=state[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d);             k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                        k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);      k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                        k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d; state[4]+=e;
    }

    void update(const uint8_t* data, std::size_t len) {
        length_bits += static_cast<uint64_t>(len) * 8;
        while (len > 0) {
            std::size_t take = std::min<std::size_t>(64 - buf_len, len);
            std::memcpy(buf + buf_len, data, take);
            buf_len += take; data += take; len -= take;
            if (buf_len == 64) { process_block(buf); buf_len = 0; }
        }
    }

    void finish(uint8_t out[20]) {
        buf[buf_len++] = 0x80;
        if (buf_len > 56) {
            while (buf_len < 64) buf[buf_len++] = 0;
            process_block(buf); buf_len = 0;
        }
        while (buf_len < 56) buf[buf_len++] = 0;
        for (int i = 7; i >= 0; --i) buf[buf_len++] = static_cast<uint8_t>(length_bits >> (i * 8));
        process_block(buf);
        for (int i = 0; i < 5; ++i) {
            out[i*4]   = static_cast<uint8_t>(state[i] >> 24);
            out[i*4+1] = static_cast<uint8_t>(state[i] >> 16);
            out[i*4+2] = static_cast<uint8_t>(state[i] >> 8);
            out[i*4+3] = static_cast<uint8_t>(state[i]);
        }
    }
};

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const uint8_t* data, std::size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= len) {
        uint32_t triple = (uint32_t(data[i]) << 16) | (uint32_t(data[i+1]) << 8) | uint32_t(data[i+2]);
        out.push_back(kB64[(triple >> 18) & 0x3F]);
        out.push_back(kB64[(triple >> 12) & 0x3F]);
        out.push_back(kB64[(triple >> 6) & 0x3F]);
        out.push_back(kB64[triple & 0x3F]);
        i += 3;
    }
    if (i < len) {
        uint32_t triple = uint32_t(data[i]) << 16;
        if (i + 1 < len) triple |= uint32_t(data[i+1]) << 8;
        out.push_back(kB64[(triple >> 18) & 0x3F]);
        out.push_back(kB64[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kB64[(triple >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

bool ieq(const std::string& a, const char* b) {
    std::size_t n = std::strlen(b);
    if (a.size() != n) return false;
    for (std::size_t i = 0; i < n; ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
        if (x != y) return false;
    }
    return true;
}

void trim_inplace(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i > 0) s.erase(0, i);
}

} // namespace

std::string compute_accept(const std::string& key) {
    static const char kMagic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    Sha1 h;
    h.update(reinterpret_cast<const uint8_t*>(key.data()), key.size());
    h.update(reinterpret_cast<const uint8_t*>(kMagic), sizeof(kMagic) - 1);
    uint8_t digest[20];
    h.finish(digest);
    return base64_encode(digest, 20);
}

bool parse_http_request(const std::string& buf, HttpRequest& out) {
    auto end = buf.find("\r\n\r\n");
    if (end == std::string::npos) return false;
    out.total_size = end + 4;

    auto line_end = buf.find("\r\n");
    if (line_end == std::string::npos) return false;
    {
        std::string line = buf.substr(0, line_end);
        std::size_t sp1 = line.find(' ');
        std::size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : line.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
        out.method = line.substr(0, sp1);
        out.path   = line.substr(sp1 + 1, sp2 - sp1 - 1);
    }

    std::size_t pos = line_end + 2;
    while (pos < end) {
        std::size_t le = buf.find("\r\n", pos);
        if (le == std::string::npos || le > end) break;
        std::string line = buf.substr(pos, le - pos);
        pos = le + 2;
        if (line.empty()) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name  = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        trim_inplace(value);
        if      (ieq(name, "Sec-WebSocket-Key")) out.sec_websocket_key = value;
        else if (ieq(name, "Upgrade"))           out.upgrade           = value;
        else if (ieq(name, "Connection"))        out.connection        = value;
    }
    return true;
}

std::string build_handshake_response(const std::string& accept) {
    std::ostringstream os;
    os << "HTTP/1.1 101 Switching Protocols\r\n"
       << "Upgrade: websocket\r\n"
       << "Connection: Upgrade\r\n"
       << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
    return os.str();
}

std::string build_http_html_response(const std::string& body) {
    std::ostringstream os;
    os << "HTTP/1.1 200 OK\r\n"
       << "Content-Type: text/html; charset=utf-8\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Cache-Control: no-store\r\n"
       << "Connection: close\r\n\r\n"
       << body;
    return os.str();
}

// Reject frames whose payload would blow the server's rx buffer. RFC 6455
// allows up to 2^63 bytes; we cap at 1 MiB which comfortably fits every
// request the inspector protocol emits. Oversized frames are treated as a
// protocol error so the caller drops the connection rather than
// accumulating bytes toward an unbounded payload.
constexpr uint64_t kMaxFramePayloadBytes = 1ull << 20; // 1 MiB

DecodedFrame decode_frame(const std::string& buf) {
    DecodedFrame r;
    if (buf.size() < 2) return r;
    const uint8_t b0 = static_cast<uint8_t>(buf[0]);
    const uint8_t b1 = static_cast<uint8_t>(buf[1]);
    r.fin = (b0 & 0x80) != 0;
    uint8_t op = b0 & 0x0F;
    if ((op > 0xA) || (op >= 0x3 && op <= 0x7) || (op >= 0xB)) {
        r.protocol_err = true; return r;
    }
    r.opcode = static_cast<WsOpcode>(op);

    bool masked = (b1 & 0x80) != 0;
    // RFC 6455 §5.1: a server MUST close the connection on any unmasked
    // frame from a client. The only client-side opcodes we accept are
    // Text/Binary/Close/Ping/Pong/Continuation — all must be masked.
    if (!masked) { r.protocol_err = true; return r; }

    uint64_t plen = b1 & 0x7F;
    std::size_t header = 2;
    if (plen == 126) {
        if (buf.size() < 4) return r;
        plen = (uint64_t(uint8_t(buf[2])) << 8) | uint8_t(buf[3]);
        header = 4;
    } else if (plen == 127) {
        if (buf.size() < 10) return r;
        plen = 0;
        for (int i = 0; i < 8; ++i) plen = (plen << 8) | uint8_t(buf[2 + i]);
        header = 10;
    }
    if (plen > kMaxFramePayloadBytes) {
        r.protocol_err = true; return r;
    }
    uint8_t mask[4] = {0,0,0,0};
    if (buf.size() < header + 4) return r;
    for (int i = 0; i < 4; ++i) mask[i] = uint8_t(buf[header + i]);
    header += 4;
    if (buf.size() < header + plen) return r;

    r.payload.resize(static_cast<std::size_t>(plen));
    for (uint64_t i = 0; i < plen; ++i) {
        r.payload[static_cast<std::size_t>(i)] = static_cast<char>(uint8_t(buf[header + i]) ^ mask[i & 3]);
    }
    r.consumed = header + static_cast<std::size_t>(plen);
    r.ok = true;
    return r;
}

std::string encode_frame(WsOpcode opcode, const std::string& payload) {
    std::string out;
    out.reserve(payload.size() + 10);
    out.push_back(static_cast<char>(0x80 | static_cast<uint8_t>(opcode)));
    std::size_t plen = payload.size();
    if (plen <= 125) {
        out.push_back(static_cast<char>(plen));
    } else if (plen <= 0xFFFF) {
        out.push_back(static_cast<char>(126));
        out.push_back(static_cast<char>((plen >> 8) & 0xFF));
        out.push_back(static_cast<char>(plen & 0xFF));
    } else {
        out.push_back(static_cast<char>(127));
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<char>((static_cast<uint64_t>(plen) >> (i * 8)) & 0xFF));
        }
    }
    out.append(payload);
    return out;
}

} // namespace ergo::inspector::ws
