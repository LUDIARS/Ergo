#pragma once
/// A request head Portus parsed (portus::parse_request_head) as the Studio's
/// net::HttpRequest, the contract every handler and plugin reads.
#include "ergo/studio/net/http_message.h"

#include <portus/http_message.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace ergo::studio::web {

/// At most this many query pairs are read (the rest of the query string is refused).
inline constexpr std::size_t kMaxQueryPairs = 256;

/// nullopt when it cannot be a Studio request: a target that is not an origin
/// form path, a bad percent escape, more than kMaxQueryPairs query pairs, or
/// more than one Host header.
std::optional<net::HttpRequest> to_request(const portus::RequestHead& head, std::string body, net::Gate gate);

/// True when the head asks for a WebSocket upgrade (GET, Upgrade: websocket, Connection: upgrade).
bool wants_websocket(const portus::RequestHead& head);

} // namespace ergo::studio::web
