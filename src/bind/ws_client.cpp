#include "ws_client.h"

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

namespace ergo::bind::ws {

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

bool send_all(socket_t s, const char* data, std::size_t len) {
    while (len > 0) {
        int n = ::send(s, data, static_cast<int>(len), 0);
        if (n > 0) { data += n; len -= static_cast<std::size_t>(n); }
        else if (n < 0 && EAGAIN_OR_WOULDBLOCK(sock_last_err())) {
            // Tiny backoff for the (rare) case the kernel buffer is full.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            return false;
        }
    }
    return true;
}

bool read_http_response(socket_t s, std::string& out, int max_iters = 200) {
    out.clear();
    char buf[1024];
    for (int i = 0; i < max_iters; ++i) {
        int n = ::recv(s, buf, sizeof(buf), 0);
        if (n > 0) {
            out.append(buf, static_cast<std::size_t>(n));
            if (out.find("\r\n\r\n") != std::string::npos) return true;
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

bool recv_blocking(socket_t s, std::string& dst, std::size_t want) {
    while (want > 0) {
        char buf[2048];
        int chunk = static_cast<int>(want < sizeof(buf) ? want : sizeof(buf));
        int n = ::recv(s, buf, chunk, 0);
        if (n > 0) { dst.append(buf, static_cast<std::size_t>(n)); want -= static_cast<std::size_t>(n); }
        else if (n == 0) return false;
        else if (EAGAIN_OR_WOULDBLOCK(sock_last_err())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } else {
            return false;
        }
    }
    return true;
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
    send_queue_.clear();
}

void Client::send_text(std::string payload) {
    if (!connected_.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lk(send_mtx_);
    send_queue_.push_back(std::move(payload));
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
    if (::connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        sock_close(s); freeaddrinfo(res); return false;
    }
    freeaddrinfo(res);

    if (!perform_handshake(static_cast<int>(s))) { sock_close(s); return false; }

    // Switch to non-blocking now so the read loop can interleave sends.
    sock_set_nonblock(s);

    connected_.store(true, std::memory_order_release);
    {
        OnOpen cb;
        { std::lock_guard<std::mutex> lk(cb_mtx_); cb = on_open_; }
        if (cb) cb();
    }

    std::string rx;
    bool ok = true;
    while (!stop_flag_.load(std::memory_order_acquire)) {
        // Drain pending sends first.
        if (!drain_send_queue(static_cast<int>(s))) { ok = false; break; }

        // Try to read one frame (non-blocking).
        std::string payload;
        uint8_t op = 0;
        bool got = read_one_frame(static_cast<int>(s), rx, payload, op);
        if (got) {
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

bool Client::perform_handshake(int sock) {
    const std::string key = random_websocket_key();
    std::string req;
    req += "GET " + path_ + " HTTP/1.1\r\n";
    req += "Host: " + host_ + ":" + std::to_string(port_) + "\r\n";
    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: " + key + "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";
    req += "\r\n";
    if (!send_all(static_cast<socket_t>(sock), req.data(), req.size())) return false;
    std::string resp;
    if (!read_http_response(static_cast<socket_t>(sock), resp)) return false;
    return resp.compare(0, 12, "HTTP/1.1 101") == 0;
}

bool Client::drain_send_queue(int sock) {
    std::deque<std::string> local;
    { std::lock_guard<std::mutex> lk(send_mtx_); local.swap(send_queue_); }
    while (!local.empty()) {
        std::string frame = encode_text_masked(local.front());
        local.pop_front();
        if (!send_all(static_cast<socket_t>(sock), frame.data(), frame.size())) return false;
    }
    return true;
}

bool Client::read_one_frame(int sock, std::string& rx, std::string& payload, uint8_t& opcode) {
    auto try_recv = [&](std::size_t want) -> bool {
        // Try non-blocking read up to `want`. Returns true if rx grew enough.
        while (rx.size() < want) {
            char buf[2048];
            int chunk = static_cast<int>(want - rx.size() < sizeof(buf) ? want - rx.size() : sizeof(buf));
            int n = ::recv(static_cast<socket_t>(sock), buf, chunk, 0);
            if (n > 0) rx.append(buf, static_cast<std::size_t>(n));
            else if (n == 0) return false;
            else if (EAGAIN_OR_WOULDBLOCK(sock_last_err())) return false;
            else return false;
        }
        return true;
    };

    if (!try_recv(2)) return false;
    const uint8_t b0 = static_cast<uint8_t>(rx[0]);
    const uint8_t b1 = static_cast<uint8_t>(rx[1]);
    opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    uint64_t plen = b1 & 0x7F;
    std::size_t header = 2;
    if (plen == 126) {
        if (!try_recv(4)) return false;
        plen = (uint64_t(uint8_t(rx[2])) << 8) | uint8_t(rx[3]);
        header = 4;
    } else if (plen == 127) {
        if (!try_recv(10)) return false;
        plen = 0;
        for (int i = 0; i < 8; ++i) plen = (plen << 8) | uint8_t(rx[2 + i]);
        header = 10;
    }
    uint8_t mask[4] = {0,0,0,0};
    if (masked) {
        if (!try_recv(header + 4)) return false;
        for (int i = 0; i < 4; ++i) mask[i] = uint8_t(rx[header + i]);
        header += 4;
    }
    if (!try_recv(header + static_cast<std::size_t>(plen))) return false;

    payload.resize(static_cast<std::size_t>(plen));
    for (uint64_t i = 0; i < plen; ++i) {
        uint8_t b = static_cast<uint8_t>(rx[header + i]);
        if (masked) b ^= mask[i & 3];
        payload[static_cast<std::size_t>(i)] = static_cast<char>(b);
    }
    rx.erase(0, header + static_cast<std::size_t>(plen));
    return true;
}

} // namespace ergo::bind::ws
