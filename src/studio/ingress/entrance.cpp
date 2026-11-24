#include "ergo/studio/ingress/entrance.h"

#include "ergo/studio/net/request_guard.h"

#include <algorithm>

namespace ergo::studio::ingress {
namespace {

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() > suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::vector<std::string>& names, const std::string& name) {
    return !name.empty() && std::find(names.begin(), names.end(), name) != names.end();
}

/// A header with a value ("" counts as absent: a proxy that forwards an empty one asserted nothing).
std::optional<std::string> present(const net::HttpRequest& req, const char* name) {
    auto v = req.header(name);
    if (v && v->empty()) return std::nullopt;
    return v;
}

/// The Host's name when it is a published-form name (no port, or :443); "" otherwise.
std::string published_form(const std::optional<std::string>& host) {
    if (!host) return {};
    const auto names = net::parse_public_hosts(*host);
    return names.size() == 1 ? names.front() : std::string();
}

Entrance local_gate(const EntrancePolicy& policy, const net::HttpRequest& req, bool websocket) {
    const auto        host = req.header("host");
    const std::string name = published_form(host);
    if (contains(policy.public_hosts, name) || contains(policy.tailnet_hosts, name)) {
        return {"published Host '" + *host + "' on the local port", {}};
    }
    const auto origin = req.header("origin");
    return {websocket ? net::ws_handshake_problem(host, origin, policy.bind_host)
                      : net::cross_site_problem(host, origin, policy.bind_host),
            {}};
}

/// `names` is the list the Host is on, `proof` the header the proxy adds.
Entrance proxied(const EntrancePolicy& policy, const std::vector<std::string>& names, const net::HttpRequest& req,
                 const char* proof, const char* proof_label, const std::optional<std::string>& who) {
    const auto host = req.header("host");
    const bool proven = present(req, proof).has_value();
    // The HTTP form for WebSocket handshakes too: a handshake without Origin still needs the proof.
    auto problem = net::cross_site_problem(net::GuardPolicy{policy.bind_host, names}, host, req.header("origin"), proven);
    if (problem && !proven) problem = "Host '" + host.value_or("") + "' without " + proof_label;
    std::string named = problem ? std::string() : who.value_or("");
    return {std::move(problem), std::move(named)};
}

Entrance remote_gate(const EntrancePolicy& policy, const net::HttpRequest& req) {
    const auto        host = req.header("host");
    const std::string name = published_form(host);
    if (contains(policy.public_hosts, name)) {
        return proxied(policy, policy.public_hosts, req, kAccessAssertionHeader, "the Cloudflare Access assertion",
                       present(req, kAccessEmailHeader).value_or("(access)"));
    }
    if (contains(policy.tailnet_hosts, name)) {
        return proxied(policy, policy.tailnet_hosts, req, kTailscaleLoginHeader, "Tailscale-User-Login",
                       present(req, kTailscaleLoginHeader));
    }
    return {"Host '" + host.value_or("") + "' is neither a public nor a tailnet name of the remote port", {}};
}

} // namespace

std::vector<std::string> parse_tailnet_hosts(const std::string& csv) {
    std::vector<std::string> out;
    for (std::string& name : net::parse_public_hosts(csv)) {
        if (ends_with(name, ".ts.net")) out.push_back(std::move(name));
    }
    return out;
}

Entrance check_entrance(const EntrancePolicy& policy, net::Gate gate, const net::HttpRequest& req, bool websocket) {
    return gate == net::Gate::local ? local_gate(policy, req, websocket) : remote_gate(policy, req);
}

} // namespace ergo::studio::ingress
