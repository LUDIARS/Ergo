#pragma once
/// The entrance check of the Studio's web server: which requests each listener
/// (net::Gate) lets in (spec/feature/tool/ergo.md "口と入口").
///
/// The local gate (127.0.0.1:<PORT>) serves this machine. It takes the names a
/// rebinding page cannot present (request_guard.h: localhost, IP literals, the
/// bind host) and refuses a published name: a public (Cloudflare) or tailnet
/// name reaching it means the tunnel points at the wrong port.
///
/// The remote gate (127.0.0.1:<ERGO_STUDIO_REMOTE_PORT>) is the target of
/// cloudflared and `tailscale serve`. It takes only
///   - a public name (ERGO_STUDIO_PUBLIC_HOSTS) with the Cloudflare Access
///     assertion (Cf-Access-Jwt-Assertion), or
///   - a tailnet name (ERGO_STUDIO_TAILNET_HOSTS, *.ts.net) with the
///     Tailscale-User-Login that `tailscale serve` adds (a Funnel request from
///     outside the tailnet has none),
/// both with no Origin or an Origin on that same name. Any other Host is refused,
/// a WebSocket handshake included (no "native client" exception remotely).
#include "ergo/studio/net/http_message.h"

#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::ingress {

/// Added by Cloudflare Access to every request it lets through.
inline constexpr const char* kAccessAssertionHeader = "cf-access-jwt-assertion";
/// The signed-in user Cloudflare Access names (logged as who).
inline constexpr const char* kAccessEmailHeader = "cf-access-authenticated-user-email";
/// The tailnet user `tailscale serve` names on every request it proxies.
inline constexpr const char* kTailscaleLoginHeader = "tailscale-user-login";

struct EntrancePolicy {
    std::string              bind_host;      ///< also a local name
    std::vector<std::string> public_hosts;   ///< exact names behind Cloudflare Access (lower-cased)
    std::vector<std::string> tailnet_hosts;  ///< exact *.ts.net names behind `tailscale serve` (lower-cased)
};

/// Comma-separated names -> the *.ts.net ones, trimmed and lower-cased (others are dropped).
std::vector<std::string> parse_tailnet_hosts(const std::string& csv);

struct Entrance {
    std::optional<std::string> problem;  ///< nullopt = let in; otherwise why it is refused (logged, 403)
    std::string                who;      ///< the Access e-mail / Tailscale login ("" on the local gate)
};

/// Decide one request (`websocket` = a WebSocket handshake) arriving on `gate`.
Entrance check_entrance(const EntrancePolicy& policy, net::Gate gate, const net::HttpRequest& req, bool websocket);

} // namespace ergo::studio::ingress
