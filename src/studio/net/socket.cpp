#include "socket.h"

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

#include <cstring>
#include <mutex>

namespace ergo::studio::net::sock {

bool init() {
#ifdef _WIN32
    static std::once_flag once;
    static bool ok = false;
    std::call_once(once, [] {
        WSADATA d;
        ok = WSAStartup(MAKEWORD(2, 2), &d) == 0;
    });
    return ok;
#else
    return true;
#endif
}

Handle listen_tcp(const std::string& host, uint16_t port, uint16_t& bound_port, std::string& err) {
    if (!init()) { err = "socket library init failed"; return kInvalid; }
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE | AI_NUMERICHOST;
    addrinfo* res = nullptr;
    const std::string port_s = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res) != 0 || !res) {
        err = "bind host must be a numeric address: " + host;
        return kInvalid;
    }
    const auto s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) { freeaddrinfo(res); err = "socket() failed"; return kInvalid; }
#ifdef _WIN32
    BOOL excl = TRUE;  // do not share the port with another listener
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&excl), sizeof(excl));
#else
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
    if (::bind(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        freeaddrinfo(res);
        close(static_cast<Handle>(s));
        err = "port " + port_s + " on " + host + " is in use";
        return kInvalid;
    }
    freeaddrinfo(res);
    if (::listen(s, 64) != 0) { close(static_cast<Handle>(s)); err = "listen() failed"; return kInvalid; }

    sockaddr_storage ss{};
    socklen_t sl = sizeof(ss);
    getsockname(s, reinterpret_cast<sockaddr*>(&ss), &sl);
    bound_port = ss.ss_family == AF_INET6 ? ntohs(reinterpret_cast<sockaddr_in6*>(&ss)->sin6_port)
                                          : ntohs(reinterpret_cast<sockaddr_in*>(&ss)->sin_port);
    return static_cast<Handle>(s);
}

Handle accept_one(Handle listener) {
    const auto c = ::accept(static_cast<decltype(::socket(0, 0, 0))>(listener), nullptr, nullptr);
#ifdef _WIN32
    if (c == INVALID_SOCKET) return kInvalid;
#else
    if (c < 0) return kInvalid;
#endif
    return static_cast<Handle>(c);
}

long recv_some(Handle s, char* buf, std::size_t len) {
    return static_cast<long>(::recv(static_cast<decltype(::socket(0, 0, 0))>(s), buf, static_cast<int>(len), 0));
}

bool send_all(Handle s, const char* data, std::size_t len) {
    std::size_t done = 0;
    while (done < len) {
        const auto n = ::send(static_cast<decltype(::socket(0, 0, 0))>(s), data + done,
                              static_cast<int>(len - done), 0);
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

void shutdown_both(Handle s) {
#ifdef _WIN32
    ::shutdown(static_cast<SOCKET>(s), SD_BOTH);
#else
    ::shutdown(static_cast<int>(s), SHUT_RDWR);
#endif
}

void close(Handle s) {
    if (s == kInvalid) return;
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(s));
#else
    ::close(static_cast<int>(s));
#endif
}

} // namespace ergo::studio::net::sock
