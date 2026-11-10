#pragma once
/// Cross-site / DNS-rebinding guard (port of tools/ergo core/request_guard.ts).
///
/// Binding to 127.0.0.1 does not stop a web page from talking to the tool:
/// a browser lets any site open ws://127.0.0.1:5170/... and send simple POSTs
/// to it, and a DNS-rebinding page reaches it under its own host name. A
/// request is accepted only when
///   - its Host is a name such a page cannot present: localhost / *.localhost,
///     an IP literal, or the configured bind host; and
///   - it has no Origin (native engine clients, curl) or an Origin whose host
///     equals that Host. An Origin of "null" is cross-site.
#include <optional>
#include <string>

namespace ergo::studio::net {

/// True when `host` ("name[:port]") is one a rebinding page cannot present.
bool is_trusted_host(const std::optional<std::string>& host, const std::string& bind_host);

/// nullopt when an HTTP request may proceed, otherwise why it is refused.
std::optional<std::string> cross_site_problem(const std::optional<std::string>& host,
                                              const std::optional<std::string>& origin,
                                              const std::string& bind_host);

/// nullopt when a WebSocket handshake may proceed. A handshake without Origin
/// is a native client (ergo_common_ws) and may dial any host name.
std::optional<std::string> ws_handshake_problem(const std::optional<std::string>& host,
                                                const std::optional<std::string>& origin,
                                                const std::string& bind_host);

} // namespace ergo::studio::net
