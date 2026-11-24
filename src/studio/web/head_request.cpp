#include "head_request.h"

#include <utility>

namespace ergo::studio::web {
namespace {

/// "a=1&b=2" into `out`; false on a bad escape or too many pairs.
bool read_query(const std::string& qs, std::map<std::string, std::string>& out) {
    std::size_t start = 0;
    std::size_t pairs = 0;
    while (start <= qs.size()) {
        const auto        amp  = qs.find('&', start);
        const std::string pair = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        if (!pair.empty()) {
            if (++pairs > kMaxQueryPairs) return false;
            const auto eq = pair.find('=');
            auto       k  = net::percent_decode(pair.substr(0, eq), true);
            auto       v  = net::percent_decode(eq == std::string::npos ? std::string() : pair.substr(eq + 1), true);
            if (!k || !v) return false;
            out[*k] = *v;
        }
        if (amp == std::string::npos) break;
        start = amp + 1;
    }
    return true;
}

} // namespace

std::optional<net::HttpRequest> to_request(const portus::RequestHead& head, std::string body, net::Gate gate) {
    if (head.target.empty() || head.target[0] != '/') return std::nullopt;
    if (head.headers.count("host") > 1) return std::nullopt;
    net::HttpRequest req;
    req.method = head.method;
    req.target = head.target;
    req.gate   = gate;
    const auto q    = head.target.find('?');
    auto       path = net::percent_decode(head.target.substr(0, q), false);
    if (!path) return std::nullopt;
    req.path = std::move(*path);
    if (q != std::string::npos && !read_query(head.target.substr(q + 1), req.query)) return std::nullopt;
    // Names are lower-cased by Portus; repeated headers join with ", " (as the old server did).
    for (const auto& [name, value] : head.headers.all()) {
        auto it = req.headers.find(name);
        if (it == req.headers.end()) req.headers.emplace(name, value);
        else it->second += ", " + value;
    }
    req.body = std::move(body);
    return req;
}

bool wants_websocket(const portus::RequestHead& head) {
    return head.method == "GET" && portus::header_has_token(head.headers.get("upgrade"), "websocket")
        && portus::header_has_token(head.headers.get("connection"), "upgrade");
}

} // namespace ergo::studio::web
