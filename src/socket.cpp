#include "ergo/network/socket.h"
#include <cstring>

#ifdef _WIN32
  #pragma comment(lib, "ws2_32.lib")
#elif defined(__linux__)
  #include <netinet/tcp.h>
#endif

namespace ergo::network {

Socket::Socket() = default;

Socket::Socket(SocketHandle handle)
    : handle_(handle)
    , state_(handle != kInvalidSocket ? SocketState::Connected : SocketState::Closed) {
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : handle_(other.handle_)
    , state_(other.state_) {
    other.handle_ = kInvalidSocket;
    other.state_ = SocketState::Closed;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        state_ = other.state_;
        other.handle_ = kInvalidSocket;
        other.state_ = SocketState::Closed;
    }
    return *this;
}

bool Socket::create(SocketProtocol protocol) {
    close();
    int type = (protocol == SocketProtocol::TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int proto = (protocol == SocketProtocol::TCP) ? IPPROTO_TCP : IPPROTO_UDP;
    handle_ = ::socket(AF_INET, type, proto);
    if (handle_ == kInvalidSocket) {
        state_ = SocketState::Error;
        return false;
    }
    state_ = SocketState::Closed;
    return true;
}

bool Socket::connect(const ResolvedAddress& addr) {
    if (handle_ == kInvalidSocket) return false;

    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(addr.port);
    if (inet_pton(AF_INET, addr.ip.c_str(), &sa.sin_addr) <= 0) {
        state_ = SocketState::Error;
        return false;
    }

    state_ = SocketState::Connecting;
    int result = ::connect(handle_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa));
    if (result == kSocketError) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            state_ = SocketState::Error;
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            state_ = SocketState::Error;
            return false;
        }
#endif
    }
    state_ = SocketState::Connected;
    return true;
}

bool Socket::bind(uint16_t port, const std::string& ip) {
    if (handle_ == kInvalidSocket) return false;

    int opt = 1;
    setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) <= 0) {
        return false;
    }

    return ::bind(handle_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) != kSocketError;
}

bool Socket::listen(int backlog) {
    if (handle_ == kInvalidSocket) return false;
    if (::listen(handle_, backlog) == kSocketError) {
        state_ = SocketState::Error;
        return false;
    }
    state_ = SocketState::Listening;
    return true;
}

Socket Socket::accept() {
    if (handle_ == kInvalidSocket) return Socket{};

    struct sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    SocketHandle client = ::accept(handle_,
                                   reinterpret_cast<struct sockaddr*>(&client_addr),
                                   &addr_len);
    return Socket{client};
}

int Socket::send(const void* data, size_t length) {
    if (handle_ == kInvalidSocket) return -1;

    const auto* ptr = static_cast<const char*>(data);
    size_t total_sent = 0;

    while (total_sent < length) {
        int sent = ::send(handle_, ptr + total_sent,
                          static_cast<int>(length - total_sent), 0);
        if (sent <= 0) {
            if (total_sent > 0) return static_cast<int>(total_sent);
            state_ = SocketState::Error;
            return -1;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return static_cast<int>(total_sent);
}

int Socket::recv(void* buffer, size_t length) {
    if (handle_ == kInvalidSocket) return -1;

    int received = ::recv(handle_, static_cast<char*>(buffer),
                          static_cast<int>(length), 0);
    if (received < 0) {
        state_ = SocketState::Error;
    } else if (received == 0) {
        state_ = SocketState::Closed;
    }
    return received;
}

void Socket::close() {
    if (handle_ != kInvalidSocket) {
#ifdef _WIN32
        ::closesocket(handle_);
#else
        ::close(handle_);
#endif
        handle_ = kInvalidSocket;
        state_ = SocketState::Closed;
    }
}

bool Socket::set_non_blocking(bool enabled) {
    if (handle_ == kInvalidSocket) return false;

#ifdef _WIN32
    u_long mode = enabled ? 1 : 0;
    return ioctlsocket(handle_, FIONBIO, &mode) != kSocketError;
#else
    int flags = fcntl(handle_, F_GETFL, 0);
    if (flags < 0) return false;
    if (enabled) flags |= O_NONBLOCK;
    else         flags &= ~O_NONBLOCK;
    return fcntl(handle_, F_SETFL, flags) >= 0;
#endif
}

bool Socket::set_keep_alive(bool enabled, int idle_seconds) {
    if (handle_ == kInvalidSocket) return false;

    int optval = enabled ? 1 : 0;
    if (setsockopt(handle_, SOL_SOCKET, SO_KEEPALIVE,
                   reinterpret_cast<const char*>(&optval), sizeof(optval)) < 0) {
        return false;
    }

#if defined(__linux__)
    if (enabled) {
        setsockopt(handle_, IPPROTO_TCP, TCP_KEEPIDLE,
                   &idle_seconds, sizeof(idle_seconds));
    }
#endif
    (void)idle_seconds; // macOS/Windows では OS 設定に依存
    return true;
}

bool Socket::set_timeout(uint32_t send_ms, uint32_t recv_ms) {
    if (handle_ == kInvalidSocket) return false;

#ifdef _WIN32
    DWORD s_timeout = send_ms;
    DWORD r_timeout = recv_ms;
    setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&s_timeout), sizeof(s_timeout));
    setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&r_timeout), sizeof(r_timeout));
#else
    struct timeval s_tv{};
    s_tv.tv_sec = send_ms / 1000;
    s_tv.tv_usec = (send_ms % 1000) * 1000;
    setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, &s_tv, sizeof(s_tv));

    struct timeval r_tv{};
    r_tv.tv_sec = recv_ms / 1000;
    r_tv.tv_usec = (recv_ms % 1000) * 1000;
    setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, &r_tv, sizeof(r_tv));
#endif
    return true;
}

SocketHandle Socket::release() {
    SocketHandle h = handle_;
    handle_ = kInvalidSocket;
    state_ = SocketState::Closed;
    return h;
}

} // namespace ergo::network
