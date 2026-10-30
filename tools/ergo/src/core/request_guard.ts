/// Cross-site / DNS-rebinding guard for the shared HTTP + WS server.
///
/// Binding to 127.0.0.1 does not stop a web page from talking to the tool:
/// a browser lets any site open ws://127.0.0.1:5170/... and send simple
/// (no-preflight) POSTs to it, and a DNS-rebinding page reaches it under
/// its own host name. A request is accepted only when
///   - its Host is a name such a page cannot present: localhost / *.localhost,
///     an IP literal, or the configured bind host (ERGO_HOST); and
///   - it has no Origin (native engine clients, curl, same-origin GET) or an
///     Origin whose host equals that Host (the tool's own UI).
/// An Origin of "null" (sandboxed iframe, data: URL) is cross-site.

import { isIP } from "node:net";

/// "name[:port]" lower-cased and with a default port dropped, or null when
/// it is not a valid host.
function normalizeHost(host: string): string | null {
    try {
        return new URL(`http://${host}`).host.toLowerCase();
    } catch {
        return null;
    }
}

/// True when `host` (a Host header value, "name[:port]") is one a
/// rebinding page cannot present.
export function isTrustedHost(host: string | undefined, bindHost?: string): boolean {
    if (!host) return false;
    const norm = normalizeHost(host);
    if (norm === null) return false;
    const name = new URL(`http://${norm}`).hostname;
    if (name === "localhost" || name.endsWith(".localhost")) return true;
    if (isIP(name.replace(/^\[|\]$/g, "")) !== 0) return true;
    return !!bindHost && name === bindHost.toLowerCase();
}

/// Null when an HTTP request may proceed, otherwise why it is refused.
export function crossSiteProblem(
    host:      string | undefined,
    origin:    string | undefined,
    bindHost?: string,
): string | null {
    if (!isTrustedHost(host, bindHost)) return `untrusted Host '${host ?? ""}'`;
    if (origin === undefined) return null;
    let originHost: string;
    try {
        originHost = new URL(origin).host.toLowerCase();
    } catch {
        return `cross-site Origin '${origin}'`;
    }
    return originHost === normalizeHost(host!) ? null : `cross-site Origin '${origin}'`;
}

/// Null when a WebSocket handshake may proceed, otherwise why it is refused.
/// A browser always sends Origin on a WebSocket handshake, so one without it
/// is a native client (an engine over ergo_common_ws) and may dial any host
/// name, e.g. the dev box's DNS name when ERGO_HOST=0.0.0.0. With an Origin
/// it gets the same check as an HTTP request.
export function wsHandshakeProblem(
    host:      string | undefined,
    origin:    string | undefined,
    bindHost?: string,
): string | null {
    return origin === undefined ? null : crossSiteProblem(host, origin, bindHost);
}
