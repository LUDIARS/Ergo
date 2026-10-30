#include "ergo/common/ws_client.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  static constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
  static int sock_close(socket_t s) { return ::closesocket(s); }
  static int sock_set_nonblock(socket_t s) {
      u_long m = 1; return ::ioctlsocket(s, FIONBIO, &m);
  }
  #define EAGAIN_OR_WOULDBLOCK(e) ((e) == WSAEWOULDBLOCK)
  static int sock_last_err() { return WSAGetLastError(); }
#else
  #include <sys/socket.h>
  #include <poll.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <errno.h>
  using socket_t = int;
  static constexpr socket_t INVALID_SOCK = -1;
  static int sock_close(socket_t s) { return ::close(s); }
  static int sock_set_nonblock(socket_t s) {
      int f = ::fcntl(s, F_GETFL, 0);
      return (f < 0) ? -1 : ::fcntl(s, F_SETFL, f | O_NONBLOCK);
  }
  #define EAGAIN_OR_WOULDBLOCK(e) ((e) == EAGAIN || (e) == EWOULDBLOCK)
  static int sock_last_err() { return errno; }
#endif

// A peer that vanished must surface as a send error, not as SIGPIPE killing
// the host (Linux: per-call flag; macOS: SO_NOSIGPIPE set in connect_once).
#ifdef MSG_NOSIGNAL
  static constexpr int kSendFlags = MSG_NOSIGNAL;
#else
  static constexpr int kSendFlags = 0;
#endif

namespace ergo::common::ws {

namespace {

#ifdef _WIN32
struct WinsockBootstrap {
    WinsockBootstrap()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WinsockBootstrap() { WSACleanup(); }
};
WinsockBootstrap& winsock_bootstrap() { static WinsockBootstrap b; return b; }
#endif

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64_encode(const uint8_t* data, std::size_t len) {
    std::string out; out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= len) {
        uint32_t t = (uint32_t(data[i]) << 16) | (uint32_t(data[i+1]) << 8) | uint32_t(data[i+2]);
        out.push_back(kB64[(t >> 18) & 0x3F]); out.push_back(kB64[(t >> 12) & 0x3F]);
        out.push_back(kB64[(t >> 6) & 0x3F]);  out.push_back(kB64[t & 0x3F]);
        i += 3;
    }
    if (i < len) {
        uint32_t t = uint32_t(data[i]) << 16;
        if (i + 1 < len) t |= uint32_t(data[i+1]) << 8;
        out.push_back(kB64[(t >> 18) & 0x3F]); out.push_back(kB64[(t >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kB64[(t >> 6) & 0x3F] : '='); out.push_back('=');
    }
    return out;
}

std::string random_websocket_key() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    uint8_t bytes[16];
    for (int i = 0; i < 16; ++i) bytes[i] = static_cast<uint8_t>(dist(rd));
    return base64_encode(bytes, 16);
}

// Gives up (false) as soon as `stop` is raised while the kernel buffer is
// full: a peer that keeps the connection open but stops reading (tool paused
// in a debugger, SIGSTOPped) must not keep the worker here, or stop() would
// block forever in join().
bool send_all(socket_t s, const std::atomic<bool>& stop, const char* data, std::size_t len) {
    while (len > 0) {
        int n = ::send(s, data, static_cast<int>(len), kSendFlags);
        if (n > 0) { data += n; len -= static_cast<std::size_t>(n); }
        else if (n < 0 && EAGAIN_OR_WOULDBLOCK(sock_last_err())) {
            if (stop.load(std::memory_order_acquire)) return false;
            // Tiny backoff while the kernel buffer is full.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            return false;
        }
    }
    return true;
}

// Waits up to `ms` for a pending non-blocking connect on `s`. Returns > 0
// once it has finished (either way: read SO_ERROR), 0 on timeout or EINTR,
// < 0 on a wait error.
int wait_connect_slice(socket_t s, int ms) {
#ifdef _WIN32
    // Windows fd_set is a counted array of handles, so any SOCKET value fits,
    // and a failed connect is reported through exceptfds. WSAPoll is avoided:
    // before Windows 10 2004 it never reported a refused connect.
    fd_set wr, ex;
    FD_ZERO(&wr); FD_ZERO(&ex);
    FD_SET(s, &wr); FD_SET(s, &ex);
    timeval tv{0, ms * 1000};
    return ::select(0, nullptr, &wr, &ex, &tv);
#else
    // poll(), not select(): FD_SET on an fd >= FD_SETSIZE (1024) is undefined
    // (glibc's _FORTIFY_SOURCE aborts), and a host that raised RLIMIT_NOFILE
    // can hand this worker such an fd. POLLERR / POLLHUP arrive unrequested.
    pollfd p{};
    p.fd     = s;
    p.events = POLLOUT;
    const int n = ::poll(&p, 1, ms);
    return (n < 0 && errno == EINTR) ? 0 : n;
#endif
}

// Connects a non-blocking socket, checking `stop` every 50 ms so stop() never
// waits on the OS connect timeout (Windows retries a refused loopback SYN for
// about 2 s before it reports the failure). Gives up after `timeout_ms`.
bool connect_abortable(socket_t s, const sockaddr* addr, int addrlen,
                       const std::atomic<bool>& stop, int timeout_ms = 5000) {
    if (::connect(s, addr, addrlen) == 0) return true;
#ifdef _WIN32
    if (WSAGetLastError() != WSAEWOULDBLOCK) return false;
#else
    if (errno != EINPROGRESS) return false;
#endif
    for (int waited = 0; waited < timeout_ms && !stop.load(std::memory_order_acquire); waited += 50) {
        const int n = wait_connect_slice(s, 50);
        if (n == 0) continue;
        if (n < 0) return false;
        int err = 0;
        socklen_t len = sizeof(err);
        if (::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len) != 0) return false;
        return err == 0;
    }
    return false;
}

// Reads the HTTP upgrade response from a non-blocking socket. A server may
// send its first frame in the same segment as the 101 response (the particle
// plugin pushes `state` right away), so bytes past the header terminator are
// handed back in `leftover` to seed the frame buffer instead of being lost.
// Gives up after ~2 s, or as soon as `stop` is raised.
bool read_http_response(socket_t s, const std::atomic<bool>& stop,
                        std::string& out, std::string& leftover, int max_iters = 200) {
    out.clear();
    leftover.clear();
    char buf[1024];
    for (int i = 0; i < max_iters && !stop.load(std::memory_order_acquire); ++i) {
        int n = ::recv(s, buf, sizeof(buf), 0);
        if (n > 0) {
            out.append(buf, static_cast<std::size_t>(n));
            const std::size_t end = out.find("\r\n\r\n");
            if (end != std::string::npos) {
                leftover = out.substr(end + 4);
                out.resize(end + 4);
                return true;
            }
        } else if (n == 0) {
            return false;
        } else if (EAGAIN_OR_WOULDBLOCK(sock_last_err())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            return false;
        }
    }
    return false;
}

// Encode a CLIENT-side text frame: FIN=1, opcode=text, masked.
std::string encode_text_masked(const std::string& payload) {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    uint8_t mask[4];
    for (int i = 0; i < 4; ++i) mask[i] = static_cast<uint8_t>(dist(rd));

    std::string out;
    out.reserve(payload.size() + 14);
    out.push_back(static_cast<char>(0x81));  // FIN | text
    const std::size_t plen = payload.size();
    if (plen <= 125) {
        out.push_back(static_cast<char>(0x80 | plen));
    } else if (plen <= 0xFFFF) {
        out.push_back(static_cast<char>(0x80 | 126));
        out.push_back(static_cast<char>((plen >> 8) & 0xFF));
        out.push_back(static_cast<char>(plen & 0xFF));
    } else {
        out.push_back(static_cast<char>(0x80 | 127));
        for (int i = 7; i >= 0; --i)
            out.push_back(static_cast<char>((static_cast<uint64_t>(plen) >> (i * 8)) & 0xFF));
    }
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>(mask[i]));
    for (std::size_t i = 0; i < plen; ++i) {
        out.push_back(static_cast<char>(static_cast<uint8_t>(payload[i]) ^ mask[i & 3]));
    }
    return out;
}

} // namespace

Client::Client() {
#ifdef _WIN32
    winsock_bootstrap();
#endif
}

Client::~Client() { stop(); }

void Client::set_on_text(OnText cb) { std::lock_guard<std::mutex> lk(cb_mtx_); on_text_ = std::move(cb); }
void Client::set_on_open(OnOpen cb) { std::lock_guard<std::mutex> lk(cb_mtx_); on_open_ = std::move(cb); }

void Client::start(std::string host, uint16_t port, std::string path) {
    if (running_.exchange(true, std::memory_order_acq_rel)) return;
    host_ = std::move(host); port_ = port; path_ = std::move(path);
    stop_flag_.store(false, std::memory_order_release);
    thread_ = std::thread([this]{ worker_loop(); });
}

void Client::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;
    stop_flag_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    connected_.store(false, std::memory_order_release);
    std::lock_guard<std::mutex> lk(send_mtx_);
    unsent_.fetch_sub(send_queue_.size(), std::memory_order_acq_rel);
    send_queue_.clear();
}

void Client::send_text(std::string payload) {
    if (!connected_.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lk(send_mtx_);
    send_queue_.push_back(std::move(payload));
    unsent_.fetch_add(1, std::memory_order_acq_rel);
}

void Client::worker_loop() {
    while (!stop_flag_.load(std::memory_order_acquire)) {
        if (!connect_once()) {
            for (int i = 0; i < 10 && !stop_flag_.load(std::memory_order_acquire); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        }
    }
}

bool Client::connect_once() {
    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    char portStr[16]; std::snprintf(portStr, sizeof(portStr), "%u", port_);
    if (getaddrinfo(host_.c_str(), portStr, &hints, &res) != 0 || !res) return false;

    socket_t s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCK) { freeaddrinfo(res); return false; }
#ifdef SO_NOSIGPIPE
    { int one = 1; ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)); }
#endif
    // Non-blocking from the start: connect and the handshake read are then
    // bounded (and abortable by stop()) even if nothing listens or the peer
    // accepts but never answers, and the read loop below can interleave sends.
    sock_set_nonblock(s);
    if (!connect_abortable(s, res->ai_addr, static_cast<int>(res->ai_addrlen), stop_flag_)) {
        sock_close(s); freeaddrinfo(res); return false;
    }
    freeaddrinfo(res);

    std::string rx;  // seeded with any frame bytes that followed the 101 response
    if (!perform_handshake(static_cast<int>(s), rx)) { sock_close(s); return false; }

    connected_.store(true, std::memory_order_release);
    {
        OnOpen cb;
        { std::lock_guard<std::mutex> lk(cb_mtx_); cb = on_open_; }
        if (cb) cb();
    }

    bool ok = true;
    while (!stop_flag_.load(std::memory_order_acquire)) {
        // Drain pending sends first.
        if (!drain_send_queue(static_cast<int>(s))) { ok = false; break; }

        // Try to read one frame (non-blocking).
        std::string payload;
        uint8_t op = 0;
        int rv = read_one_frame(static_cast<int>(s), rx, payload, op);
        if (rv < 0) {
            // Peer closed the TCP stream without a close frame, or a protocol
            // violation (e.g. oversized frame). Drop the connection and let
            // the worker back off before reconnecting.
            ok = false;
            break;
        }
        if (rv == 1) {
            if (op == 0x1) {
                OnText cb;
                { std::lock_guard<std::mutex> lk(cb_mtx_); cb = on_text_; }
                if (cb) cb(payload);
            } else if (op == 0x8) { ok = true; break; } // server close
            // Ping/Pong/Binary/Continuation: ignored
        } else {
            // No frame ready — short sleep to avoid burning the CPU.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    connected_.store(false, std::memory_order_release);
    sock_close(s);
    return ok;
}

bool Client::perform_handshake(int sock, std::string& leftover) {
    const std::string key = random_websocket_key();
    std::string req;
    req += "GET " + path_ + " HTTP/1.1\r\n";
    req += "Host: " + host_ + ":" + std::to_string(port_) + "\r\n";
    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: " + key + "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";
    req += "\r\n";
    if (!send_all(static_cast<socket_t>(sock), stop_flag_, req.data(), req.size())) return false;
    std::string resp;
    if (!read_http_response(static_cast<socket_t>(sock), stop_flag_, resp, leftover)) return false;
    return resp.compare(0, 12, "HTTP/1.1 101") == 0;
}

bool Client::drain_send_queue(int sock) {
    std::deque<std::string> local;
    { std::lock_guard<std::mutex> lk(send_mtx_); local.swap(send_queue_); }
    while (!local.empty()) {
        bool ok = !stop_flag_.load(std::memory_order_acquire);
        if (ok) {
            const std::string frame = encode_text_masked(local.front());
            ok = send_all(static_cast<socket_t>(sock), stop_flag_, frame.data(), frame.size());
        }
        if (!ok) {  // the rest of `local` is lost with the session
            unsent_.fetch_sub(local.size(), std::memory_order_acq_rel);
            return false;
        }
        local.pop_front();
        unsent_.fetch_sub(1, std::memory_order_acq_rel);
    }
    return true;
}

// Any frame larger than this is almost certainly a hostile or misconfigured
// peer; we refuse rather than allowing rx to balloon to the attacker-declared
// length. Tool payloads (scene JSON, particle config, log batches) stay far
// below it.
static constexpr uint64_t kMaxFramePayloadBytes = 1ull << 20; // 1 MiB

int Client::read_one_frame(int sock, std::string& rx, std::string& payload, uint8_t& opcode) {
    bool dead = false;  // peer closed the stream or the socket errored
    auto try_recv = [&](std::size_t want) -> bool {
        // Try non-blocking read up to `want`. Returns true if rx grew enough.
        while (rx.size() < want) {
            char buf[2048];
            int chunk = static_cast<int>(want - rx.size() < sizeof(buf) ? want - rx.size() : sizeof(buf));
            int n = ::recv(static_cast<socket_t>(sock), buf, chunk, 0);
            if (n > 0) rx.append(buf, static_cast<std::size_t>(n));
            else if (n < 0 && EAGAIN_OR_WOULDBLOCK(sock_last_err())) return false;
            else { dead = true; return false; }
        }
        return true;
    };
    // Not enough bytes yet: 0 (wait for more), unless the stream is gone.
    auto need_more = [&]() { return dead ? -1 : 0; };

    if (!try_recv(2)) return need_more();
    const uint8_t b0 = static_cast<uint8_t>(rx[0]);
    const uint8_t b1 = static_cast<uint8_t>(rx[1]);
    opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    uint64_t plen = b1 & 0x7F;
    std::size_t header = 2;
    if (plen == 126) {
        if (!try_recv(4)) return need_more();
        plen = (uint64_t(uint8_t(rx[2])) << 8) | uint8_t(rx[3]);
        header = 4;
    } else if (plen == 127) {
        if (!try_recv(10)) return need_more();
        plen = 0;
        for (int i = 0; i < 8; ++i) plen = (plen << 8) | uint8_t(rx[2 + i]);
        header = 10;
    }
    if (plen > kMaxFramePayloadBytes) return -1;
    uint8_t mask[4] = {0,0,0,0};
    if (masked) {
        if (!try_recv(header + 4)) return need_more();
        for (int i = 0; i < 4; ++i) mask[i] = uint8_t(rx[header + i]);
        header += 4;
    }
    if (!try_recv(header + static_cast<std::size_t>(plen))) return need_more();

    payload.resize(static_cast<std::size_t>(plen));
    for (uint64_t i = 0; i < plen; ++i) {
        uint8_t b = static_cast<uint8_t>(rx[header + i]);
        if (masked) b ^= mask[i & 3];
        payload[static_cast<std::size_t>(i)] = static_cast<char>(b);
    }
    rx.erase(0, header + static_cast<std::size_t>(plen));
    return 1;
}

} // namespace ergo::common::ws
