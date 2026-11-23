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
///
/// Published through an authenticating proxy (Cloudflare Access + a tunnel to
/// the loopback listener), the Studio also answers under the public names in
/// GuardPolicy::public_hosts, but only for requests that carry the proxy's
/// identity assertion header (`Cf-Access-Jwt-Assertion`): a request under a
/// public name without it did not come through Access and is refused. The
/// listener itself stays on loopback (the tunnel is the only way in).
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::net {

/// Header the authenticating proxy (Cloudflare Access) adds to every request it lets through.
inline constexpr const char* kAccessAssertionHeader = "cf-access-jwt-assertion";

struct GuardPolicy {
    std::string              bind_host;      ///< also trusted as a Host name
    std::vector<std::string> public_hosts;   ///< exact names served through Access (lower-cased on use)
};

/// Comma-separated host names -> trimmed, lower-cased, without empty entries or ports.
std::vector<std::string> parse_public_hosts(const std::string& csv);

/// nullopt when an HTTP request may proceed. `access_asserted` = the request
/// has the kAccessAssertionHeader (needed under a public host name).
std::optional<std::string> cross_site_problem(const GuardPolicy& policy, const std::optional<std::string>& host,
                                              const std::optional<std::string>& origin, bool access_asserted);

/// WebSocket form of the above (no Origin = native client, any host).
std::optional<std::string> ws_handshake_problem(const GuardPolicy& policy, const std::optional<std::string>& host,
                                                const std::optional<std::string>& origin, bool access_asserted);

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
