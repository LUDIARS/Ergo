#include "ergo/studio/net/request_guard.h"

#include <algorithm>
#include <cctype>

namespace ergo::studio::net {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

struct HostPort {
    std::string name;  // lower-cased; IPv6 keeps its brackets
    std::string port;  // empty when absent or default
};

bool valid_reg_name(const std::string& n) {
    if (n.empty()) return false;
    return std::all_of(n.begin(), n.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '.' || c == '_';
    });
}

bool is_ipv4(const std::string& s) {
    int parts = 0;
    std::size_t i = 0;
    while (i <= s.size()) {
        std::size_t j = i;
        while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
        if (j == i || j - i > 3) return false;
        if (std::stoi(s.substr(i, j - i)) > 255) return false;
        if (j - i > 1 && s[i] == '0') return false;
        ++parts;
        if (j == s.size()) break;
        if (s[j] != '.') return false;
        i = j + 1;
    }
    return parts == 4;
}

bool is_ipv6_bracketed(const std::string& s) {
    if (s.size() < 4 || s.front() != '[' || s.back() != ']') return false;
    const std::string inner = s.substr(1, s.size() - 2);
    int colons = 0;
    for (unsigned char c : inner) {
        if (c == ':') ++colons;
        else if (!std::isxdigit(c) && c != '.') return false;
    }
    return colons >= 2;
}

/// Parse "name[:port]" with `default_port` dropped. nullopt when invalid.
std::optional<HostPort> parse_host(const std::string& raw, const std::string& default_port) {
    const std::string s = lower(raw);
    HostPort hp;
    std::string rest;
    if (!s.empty() && s[0] == '[') {
        const auto close = s.find(']');
        if (close == std::string::npos) return std::nullopt;
        hp.name = s.substr(0, close + 1);
        rest = s.substr(close + 1);
        if (!is_ipv6_bracketed(hp.name)) return std::nullopt;
    } else {
        const auto colon = s.find(':');
        hp.name = s.substr(0, colon);
        rest = colon == std::string::npos ? std::string{} : s.substr(colon);
        if (!valid_reg_name(hp.name)) return std::nullopt;
    }
    if (!rest.empty()) {
        if (rest[0] != ':') return std::nullopt;
        hp.port = rest.substr(1);
        if (hp.port.empty() || hp.port.size() > 5
            || !std::all_of(hp.port.begin(), hp.port.end(), [](unsigned char c) { return std::isdigit(c); })
            || std::stoi(hp.port) > 65535) {
            return std::nullopt;
        }
        hp.port = std::to_string(std::stoi(hp.port));
        if (hp.port == default_port) hp.port.clear();
    }
    return hp;
}

std::string joined(const HostPort& hp) { return hp.port.empty() ? hp.name : hp.name + ":" + hp.port; }

bool ends_with(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

/// Host part of an Origin ("scheme://host[:port]"), default port dropped.
std::optional<std::string> origin_host(const std::string& origin) {
    const std::string o = lower(origin);
    const auto sep = o.find("://");
    if (sep == std::string::npos || sep == 0) return std::nullopt;
    const std::string scheme = o.substr(0, sep);
    std::string authority = o.substr(sep + 3);
    const auto slash = authority.find('/');
    if (slash != std::string::npos) authority = authority.substr(0, slash);
    if (authority.find('@') != std::string::npos) return std::nullopt;
    const std::string def = scheme == "https" || scheme == "wss" ? "443" : scheme == "http" || scheme == "ws" ? "80" : "";
    if (def.empty()) return std::nullopt;
    const auto hp = parse_host(authority, def);
    if (!hp) return std::nullopt;
    return joined(*hp);
}

} // namespace

bool is_trusted_host(const std::optional<std::string>& host, const std::string& bind_host) {
    if (!host || host->empty()) return false;
    const auto hp = parse_host(*host, "80");
    if (!hp) return false;
    if (hp->name == "localhost" || ends_with(hp->name, ".localhost")) return true;
    if (is_ipv4(hp->name) || is_ipv6_bracketed(hp->name)) return true;
    return !bind_host.empty() && hp->name == lower(bind_host);
}

std::optional<std::string> cross_site_problem(const std::optional<std::string>& host,
                                              const std::optional<std::string>& origin,
                                              const std::string& bind_host) {
    if (!is_trusted_host(host, bind_host)) return "untrusted Host '" + host.value_or("") + "'";
    if (!origin) return std::nullopt;
    const auto oh = origin_host(*origin);
    const auto hh = parse_host(*host, "80");
    if (!oh || !hh || *oh != joined(*hh)) return "cross-site Origin '" + *origin + "'";
    return std::nullopt;
}

std::optional<std::string> ws_handshake_problem(const std::optional<std::string>& host,
                                                const std::optional<std::string>& origin,
                                                const std::string& bind_host) {
    if (!origin) return std::nullopt;
    return cross_site_problem(host, origin, bind_host);
}

} // namespace ergo::studio::net
