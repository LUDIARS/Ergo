#pragma once
/// Byte codecs the WebSocket handshake needs (RFC 6455 §4.2.2): SHA-1 of the
/// client key + GUID, then base64. Pure functions, no I/O.
#include <array>
#include <cstdint>
#include <string>

namespace ergo::studio::net {

/// SHA-1 digest of `data` (FIPS 180-4). Used only for the handshake, never
/// for anything security-relevant.
std::array<uint8_t, 20> sha1(const std::string& data);

/// Standard base64 with `=` padding.
std::string base64_encode(const uint8_t* data, std::size_t len);

/// `Sec-WebSocket-Accept` for a client's `Sec-WebSocket-Key`.
std::string websocket_accept_key(const std::string& client_key);

} // namespace ergo::studio::net
