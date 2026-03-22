#include "ergo/network/dns_resolver.h"
#include <cstring>

namespace ergo::network {

std::vector<ResolvedAddress> DnsResolver::resolve(const std::string& host, uint16_t port) {
    std::vector<ResolvedAddress> results;

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);

    int status = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (status != 0 || res == nullptr) {
        return results;
    }

    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        ResolvedAddress addr;
        addr.port = port;
        addr.family = p->ai_family;

        char ip_buf[INET6_ADDRSTRLEN]{};
        if (p->ai_family == AF_INET) {
            auto* sa = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            inet_ntop(AF_INET, &sa->sin_addr, ip_buf, sizeof(ip_buf));
        } else if (p->ai_family == AF_INET6) {
            auto* sa = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
            inet_ntop(AF_INET6, &sa->sin6_addr, ip_buf, sizeof(ip_buf));
        } else {
            continue;
        }

        addr.ip = ip_buf;
        results.push_back(std::move(addr));
    }

    freeaddrinfo(res);
    return results;
}

bool DnsResolver::resolve_first(const std::string& host, uint16_t port, ResolvedAddress& out) {
    auto results = resolve(host, port);
    if (results.empty()) return false;
    out = results.front();
    return true;
}

} // namespace ergo::network
