#include "ergo/package_watch/loopback_url.h"

#include <cctype>

namespace ergo::package_watch {

namespace {

std::string lower(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (const char ch : s) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    }
    return true;
}

/// 127.a.b.c (各 0〜255)。
bool is_loopback_ipv4(const std::string& host) {
    int octets = 0;
    std::size_t at = 0;
    while (at <= host.size()) {
        const std::size_t dot = host.find('.', at);
        const std::string part = host.substr(at, dot == std::string::npos ? std::string::npos : dot - at);
        if (!all_digits(part) || part.size() > 3 || std::stoi(part) > 255) return false;
        if (octets == 0 && part != "127") return false;
        ++octets;
        if (dot == std::string::npos) break;
        at = dot + 1;
    }
    return octets == 4;
}

} // namespace

bool is_loopback_url(const std::string& url) {
    const std::string text = lower(url);
    std::size_t start = 0;
    if (text.rfind("http://", 0) == 0) start = 7;
    else if (text.rfind("https://", 0) == 0) start = 8;
    else return false;

    const std::size_t end = text.find_first_of("/?#", start);
    const std::string authority = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (authority.empty() || authority.find('@') != std::string::npos) return false;

    std::string host;
    std::string port;
    if (authority[0] == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string::npos) return false;
        host = authority.substr(1, close - 1);
        const std::string rest = authority.substr(close + 1);
        if (!rest.empty()) {
            if (rest[0] != ':') return false;
            port = rest.substr(1);
            if (!all_digits(port)) return false;
        }
        return host == "::1";
    }
    const std::size_t colon = authority.find(':');
    host = authority.substr(0, colon);
    if (colon != std::string::npos) {
        port = authority.substr(colon + 1);
        if (!all_digits(port)) return false;
    }
    return host == "localhost" || is_loopback_ipv4(host);
}

} // namespace ergo::package_watch
