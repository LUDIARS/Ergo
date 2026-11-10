#include "mutation_guard.h"

#include <algorithm>
#include <cctype>

namespace ergo::studio::plugins::studio {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool loopback(const std::string& h) { return h == "localhost" || h == "127.0.0.1" || h == "[::1]" || h == "::1"; }

/// "host:port" -> host ("[::1]:5170" -> "[::1]").
std::string host_only(const std::string& host_port) {
    std::string v = lower(host_port);
    const auto b = v.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    v = v.substr(b, v.find_last_not_of(" \t") - b + 1);
    if (!v.empty() && v[0] == '[') return v.substr(0, v.find(']') + 1);
    const auto i = v.rfind(':');
    return i == std::string::npos ? v : v.substr(0, i);
}

struct Origin {
    std::string scheme, host, port;
};

/// scheme://host[:port][/...]; nullopt when malformed.
std::optional<Origin> parse_origin(const std::string& o) {
    const auto sep = o.find("://");
    if (sep == std::string::npos || sep == 0) return std::nullopt;
    Origin out;
    out.scheme = lower(o.substr(0, sep));
    std::string rest = o.substr(sep + 3);
    rest = rest.substr(0, rest.find_first_of("/?#"));
    if (rest.empty()) return std::nullopt;
    std::string host = rest, port;
    if (rest[0] == '[') {
        const auto close = rest.find(']');
        if (close == std::string::npos) return std::nullopt;
        host = rest.substr(0, close + 1);
        if (close + 1 < rest.size()) {
            if (rest[close + 1] != ':') return std::nullopt;
            port = rest.substr(close + 2);
        }
    } else if (const auto c = rest.rfind(':'); c != std::string::npos) {
        host = rest.substr(0, c);
        port = rest.substr(c + 1);
    }
    if (!port.empty() && port.find_first_not_of("0123456789") != std::string::npos) return std::nullopt;
    out.host = lower(host);
    out.port = port;
    return out;
}

} // namespace

std::optional<std::string> check_mutation(const std::string& method_in, const std::optional<std::string>& host,
                                          const std::optional<std::string>& origin,
                                          const std::optional<std::string>& content_type) {
    std::string method = method_in;
    std::transform(method.begin(), method.end(), method.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (method == "GET" || method == "HEAD" || method == "OPTIONS") return std::nullopt;
    if (!host || !loopback(host_only(*host))) return std::string("loopback 以外の Host");
    if (origin) {
        const auto o = parse_origin(*origin);
        if (!o) return std::string("Origin が不正");
        if (!loopback(o->host)) return std::string("別サイトからの呼び出し");
        const std::string port = !o->port.empty() ? o->port : (o->scheme == "https" ? "443" : "80");
        const std::string h    = lower(*host);
        const std::size_t hl   = host_only(*host).size();
        std::string host_port  = hl + 1 <= h.size() ? h.substr(hl + 1) : std::string();
        if (host_port.empty()) host_port = "80";
        if (port != host_port) return std::string("別ポートからの呼び出し");
    }
    if (method != "DELETE" && lower(content_type.value_or("")).rfind("application/json", 0) != 0) {
        return std::string("JSON 以外の送信");
    }
    return std::nullopt;
}

} // namespace ergo::studio::plugins::studio
