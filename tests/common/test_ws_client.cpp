// ergo_common_ws — loopback tests for the shared WebSocket client.
//
// Each test runs a tiny in-process WS server on 127.0.0.1 with an ephemeral
// port (bind port 0, read back via getsockname), so nothing depends on a
// fixed port or on tools/ergo. Waits are deadline-bounded polls, never fixed
// sleeps standing in for "long enough".

#include "ergo/common/ws_client.h"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
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
  using sock_t = SOCKET;
  static const sock_t kBadSock = INVALID_SOCKET;
  static void close_sock(sock_t s) { ::closesocket(s); }
  using socklen_type = int;
#else
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using sock_t = int;
  static const sock_t kBadSock = -1;
  static void close_sock(sock_t s) { ::close(s); }
  using socklen_type = socklen_t;
#endif

using ergo::common::ws::Client;
using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

namespace {

#ifdef _WIN32
struct WsaInit {
    WsaInit()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaInit() { WSACleanup(); }
} g_wsa_init;
#endif

// Generous upper bounds: a healthy run finishes each wait in milliseconds.
// The reconnect bound covers the client's ~1.5 s back-off after a drop.
constexpr milliseconds kWait{3000};
constexpr milliseconds kReconnectWait{6000};

template <class Pred>
bool wait_until(Pred pred, milliseconds timeout = kWait) {
    const auto deadline = Clock::now() + timeout;
    while (Clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(milliseconds(2));
    }
    return pred();
}

/// True when `s` becomes readable before the deadline.
bool readable(sock_t s, milliseconds timeout) {
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(s, &rf);
    timeval tv{};
    tv.tv_sec  = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    return ::select(static_cast<int>(s + 1), &rf, nullptr, nullptr, &tv) > 0;
}

/// Minimal blocking WebSocket server end: accepts one client at a time,
/// answers the upgrade, and speaks unmasked server frames.
class LoopbackServer {
public:
    LoopbackServer() {
        listen_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;  // ephemeral
        ok_ = listen_ != kBadSock
           && ::bind(listen_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0
           && ::listen(listen_, 4) == 0;
        socklen_type len = sizeof(addr);
        if (ok_ && ::getsockname(listen_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
            port_ = ntohs(addr.sin_port);
        }
    }
    ~LoopbackServer() {
        drop_peer();
        if (listen_ != kBadSock) close_sock(listen_);
    }

    bool     ok()   const { return ok_ && port_ != 0; }
    uint16_t port() const { return port_; }

    /// Accept the next client and read its upgrade request. When
    /// `send_handshake` is false the caller writes the 101 itself.
    bool accept_client(milliseconds timeout = kWait, bool send_handshake = true) {
        drop_peer();
        if (!readable(listen_, timeout)) return false;
        peer_ = ::accept(listen_, nullptr, nullptr);
        if (peer_ == kBadSock) return false;
        request_.clear();
        while (request_.find("\r\n\r\n") == std::string::npos) {
            std::string chunk;
            if (!recv_some(chunk, 4096, kWait)) return false;
            request_ += chunk;
        }
        return !send_handshake || send_raw(handshake_response());
    }

    static std::string handshake_response() {
        return "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: test\r\n\r\n";
    }

    const std::string& request() const { return request_; }

    bool send_raw(const std::string& bytes) {
        std::size_t off = 0;
        while (off < bytes.size()) {
            int n = ::send(peer_, bytes.data() + off, static_cast<int>(bytes.size() - off), 0);
            if (n <= 0) return false;
            off += static_cast<std::size_t>(n);
        }
        return true;
    }

    /// Unmasked server frame with the right length encoding (7 / 16 / 64 bit).
    static std::string frame(uint8_t opcode, const std::string& payload) {
        std::string out;
        out.push_back(static_cast<char>(0x80 | opcode));
        const uint64_t n = payload.size();
        if (n <= 125) {
            out.push_back(static_cast<char>(n));
        } else if (n <= 0xFFFF) {
            out.push_back(static_cast<char>(126));
            out.push_back(static_cast<char>((n >> 8) & 0xFF));
            out.push_back(static_cast<char>(n & 0xFF));
        } else {
            out.push_back(static_cast<char>(127));
            for (int i = 7; i >= 0; --i) out.push_back(static_cast<char>((n >> (i * 8)) & 0xFF));
        }
        return out + payload;
    }

    bool send_text(const std::string& payload) { return send_raw(frame(0x1, payload)); }
    bool send_close() { return send_raw(frame(0x8, std::string())); }

    struct Frame {
        bool        ok     = false;
        bool        masked = false;
        uint8_t     opcode = 0;
        std::string payload;
    };

    /// Read one client frame, unmasking it.
    Frame read_frame(milliseconds timeout = kWait) {
        Frame f;
        std::string hdr;
        if (!recv_exact(hdr, 2, timeout)) return f;
        f.opcode = static_cast<uint8_t>(hdr[0]) & 0x0F;
        f.masked = (static_cast<uint8_t>(hdr[1]) & 0x80) != 0;
        uint64_t len = static_cast<uint8_t>(hdr[1]) & 0x7F;
        if (len == 126 || len == 127) {
            std::string ext;
            const std::size_t bytes = len == 126 ? 2 : 8;
            if (!recv_exact(ext, bytes, timeout)) return f;
            len = 0;
            for (char c : ext) len = (len << 8) | static_cast<uint8_t>(c);
        }
        std::string mask;
        if (f.masked && !recv_exact(mask, 4, timeout)) return f;
        if (!recv_exact(f.payload, static_cast<std::size_t>(len), timeout)) return f;
        if (f.masked) {
            for (std::size_t i = 0; i < f.payload.size(); ++i) {
                f.payload[i] = static_cast<char>(static_cast<uint8_t>(f.payload[i]) ^
                                                 static_cast<uint8_t>(mask[i & 3]));
            }
        }
        f.ok = true;
        return f;
    }

    /// True once the client has closed its end of the current connection.
    bool peer_closed(milliseconds timeout = kWait) {
        const auto deadline = Clock::now() + timeout;
        while (Clock::now() < deadline) {
            if (!readable(peer_, milliseconds(20))) continue;
            char buf[512];
            int n = ::recv(peer_, buf, sizeof(buf), 0);
            if (n <= 0) return true;
        }
        return false;
    }

    void drop_peer() {
        if (peer_ != kBadSock) { close_sock(peer_); peer_ = kBadSock; }
    }

private:
    bool recv_some(std::string& out, std::size_t max, milliseconds timeout) {
        if (!readable(peer_, timeout)) return false;
        std::vector<char> buf(max);
        int n = ::recv(peer_, buf.data(), static_cast<int>(max), 0);
        if (n <= 0) return false;
        out.append(buf.data(), static_cast<std::size_t>(n));
        return true;
    }

    bool recv_exact(std::string& out, std::size_t want, milliseconds timeout) {
        const auto deadline = Clock::now() + timeout;
        while (out.size() < want) {
            const auto left = std::chrono::duration_cast<milliseconds>(deadline - Clock::now());
            if (left.count() <= 0) return false;
            if (!recv_some(out, want - out.size(), left)) return false;
        }
        return true;
    }

    sock_t      listen_ = kBadSock;
    sock_t      peer_   = kBadSock;
    uint16_t    port_   = 0;
    bool        ok_     = false;
    std::string request_;
};

/// Thread-safe sink for the client's worker-thread callbacks.
struct Recorder {
    std::mutex               mtx;
    std::vector<std::string> texts;
    std::atomic<int>         opens{0};

    void attach(Client& c) {
        c.set_on_open([this] { opens.fetch_add(1); });
        c.set_on_text([this](const std::string& t) {
            std::lock_guard<std::mutex> lk(mtx);
            texts.push_back(t);
        });
    }
    std::size_t text_count() {
        std::lock_guard<std::mutex> lk(mtx);
        return texts.size();
    }
    std::string text(std::size_t i) {
        std::lock_guard<std::mutex> lk(mtx);
        return i < texts.size() ? texts[i] : std::string();
    }
};

} // namespace

TEST(WsClient, HandshakeOpensConnection) {
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client   client;
    Recorder rec;
    rec.attach(client);
    client.start("127.0.0.1", srv.port(), "/variable/ws");

    ASSERT_TRUE(srv.accept_client());
    const std::string& req = srv.request();
    EXPECT_EQ(req.rfind("GET /variable/ws HTTP/1.1\r\n", 0), 0u);
    EXPECT_NE(req.find("Upgrade: websocket\r\n"), std::string::npos);
    EXPECT_NE(req.find("Sec-WebSocket-Key: "), std::string::npos);
    EXPECT_NE(req.find("Sec-WebSocket-Version: 13\r\n"), std::string::npos);

    EXPECT_TRUE(wait_until([&] { return rec.opens.load() == 1; }));
    EXPECT_TRUE(client.is_connected());
    client.stop();
    EXPECT_FALSE(client.is_connected());
}

TEST(WsClient, SendTextArrivesMaskedAndUnmasks) {
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client client;
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return client.is_connected(); }));

    // One payload per length encoding: 7-bit, 16-bit and 64-bit.
    const std::string small = "{\"op\":\"hello\"}";
    const std::string medium(300, 'm');
    std::string large(70000, ' ');
    for (std::size_t i = 0; i < large.size(); ++i) large[i] = static_cast<char>('a' + i % 26);

    client.send_text(small);
    client.send_text(medium);
    client.send_text(large);
    const std::vector<const std::string*> wants{&small, &medium, &large};
    for (const std::string* want : wants) {
        LoopbackServer::Frame f = srv.read_frame();
        ASSERT_TRUE(f.ok);
        EXPECT_TRUE(f.masked);       // RFC 6455: client frames are always masked
        EXPECT_EQ(f.opcode, 0x1);
        EXPECT_EQ(f.payload.size(), want->size());
        EXPECT_TRUE(f.payload == *want);
    }
}

TEST(WsClient, ServerTextReachesOnText) {
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client   client;
    Recorder rec;
    rec.attach(client);
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return rec.opens.load() == 1; }));

    const std::string big(1000, 'x');
    ASSERT_TRUE(srv.send_text("{\"op\":\"set\"}"));
    ASSERT_TRUE(srv.send_text(big));
    ASSERT_TRUE(wait_until([&] { return rec.text_count() == 2; }));
    EXPECT_EQ(rec.text(0), "{\"op\":\"set\"}");
    EXPECT_TRUE(rec.text(1) == big);
}

TEST(WsClient, FrameInSameSegmentAsHandshakeIsKept) {
    // A server may push its first frame together with the 101 response (the
    // particle plugin sends `state` immediately). It must not be swallowed
    // by the handshake read.
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client   client;
    Recorder rec;
    rec.attach(client);
    client.start("127.0.0.1", srv.port(), "/particle/ws");
    ASSERT_TRUE(srv.accept_client(kWait, /*send_handshake=*/false));
    ASSERT_TRUE(srv.send_raw(LoopbackServer::handshake_response() +
                             LoopbackServer::frame(0x1, "{\"op\":\"state\"}") +
                             LoopbackServer::frame(0x1, "second")));
    ASSERT_TRUE(wait_until([&] { return rec.text_count() == 2; }));
    EXPECT_EQ(rec.text(0), "{\"op\":\"state\"}");
    EXPECT_EQ(rec.text(1), "second");
}

TEST(WsClient, CloseFrameDisconnectsAndReconnects) {
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client   client;
    Recorder rec;
    rec.attach(client);
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return rec.opens.load() == 1; }));

    ASSERT_TRUE(srv.send_close());
    EXPECT_TRUE(srv.peer_closed());
    // Second accept: the client dialled back in on its own.
    ASSERT_TRUE(srv.accept_client(kReconnectWait));
    EXPECT_TRUE(wait_until([&] { return rec.opens.load() == 2; }));
    EXPECT_TRUE(client.is_connected());
}

TEST(WsClient, PeerDroppingTcpReconnects) {
    // tools/ergo restarting closes the TCP stream without a close frame.
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client   client;
    Recorder rec;
    rec.attach(client);
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return rec.opens.load() == 1; }));

    srv.drop_peer();
    EXPECT_TRUE(wait_until([&] { return !client.is_connected(); }));
    ASSERT_TRUE(srv.accept_client(kReconnectWait));
    EXPECT_TRUE(wait_until([&] { return rec.opens.load() == 2; }));
}

TEST(WsClient, OversizedFrameDropsConnectionAndReconnects) {
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client   client;
    Recorder rec;
    rec.attach(client);
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return rec.opens.load() == 1; }));

    // Header only: announces 2 MiB (> 1 MiB cap). The client must refuse it
    // from the header alone instead of waiting for / buffering the payload.
    std::string hdr;
    hdr.push_back(static_cast<char>(0x81));
    hdr.push_back(static_cast<char>(127));
    const uint64_t n = 2ull << 20;
    for (int i = 7; i >= 0; --i) hdr.push_back(static_cast<char>((n >> (i * 8)) & 0xFF));
    ASSERT_TRUE(srv.send_raw(hdr));

    EXPECT_TRUE(srv.peer_closed());
    EXPECT_EQ(rec.text_count(), 0u);
    ASSERT_TRUE(srv.accept_client(kReconnectWait));
    EXPECT_TRUE(wait_until([&] { return rec.opens.load() == 2; }));
}

TEST(WsClient, SendWhileDisconnectedIsDropped) {
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client client;
    client.send_text("before start");  // not connected: dropped, not queued
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return client.is_connected(); }));
    client.send_text("after open");
    LoopbackServer::Frame f = srv.read_frame();
    ASSERT_TRUE(f.ok);
    EXPECT_EQ(f.payload, "after open");
}

TEST(WsClient, UnsentCountsFramesUntilWritten) {
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client client;
    EXPECT_EQ(client.unsent(), 0u);
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return client.is_connected(); }));

    // A draining peer: the count returns to zero once the frames are written.
    for (int i = 0; i < 3; ++i) client.send_text("f" + std::to_string(i));
    for (int i = 0; i < 3; ++i) ASSERT_TRUE(srv.read_frame().ok);
    EXPECT_TRUE(wait_until([&] { return client.unsent() == 0; }));

    // A peer that stops reading: frames back up and stay counted...
    const std::string chunk(64 * 1024, 'x');
    for (int i = 0; i < 1024; ++i) client.send_text(chunk);  // 64 MiB: well past any socket buffer
    std::this_thread::sleep_for(milliseconds(200));
    EXPECT_GT(client.unsent(), 0u);
    // ...until stop() drops them.
    client.stop();
    EXPECT_EQ(client.unsent(), 0u);
}

TEST(WsClient, StopReturnsPromptlyWithoutServer) {
    // Reserve an ephemeral port, then release it so nothing listens there.
    uint16_t port = 0;
    {
        LoopbackServer probe;
        ASSERT_TRUE(probe.ok());
        port = probe.port();
    }
    Client client;
    client.start("127.0.0.1", port, "/ws");
    // Let the worker fail its first connect and enter the back-off loop.
    std::this_thread::sleep_for(milliseconds(50));
    EXPECT_FALSE(client.is_connected());

    const auto t0 = Clock::now();
    client.stop();
    const auto took = std::chrono::duration_cast<milliseconds>(Clock::now() - t0);
    EXPECT_LT(took.count(), 1000);
}

#ifndef _WIN32
TEST(WsClient, StopReturnsPromptlyWhileConnecting) {
    // A refused port fails at once on POSIX, so the test above never has a
    // connect in flight there (on Windows it does: a refused SYN is retried
    // for about 2 s). Fill a listener's accept queue (backlog 0, never
    // accepts) so the kernel drops further SYNs and connect() stays pending,
    // as against a host that silently drops SYNs.
    sock_t ls = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(ls, kBadSock);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(::bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    ASSERT_EQ(::listen(ls, 0), 0);
    socklen_type len = sizeof(addr);
    ASSERT_EQ(::getsockname(ls, reinterpret_cast<sockaddr*>(&addr), &len), 0);
    std::vector<sock_t> fillers;
    for (int i = 0; i < 4; ++i) {
        sock_t f = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(f, kBadSock);
        ::fcntl(f, F_SETFL, ::fcntl(f, F_GETFL, 0) | O_NONBLOCK);
        (void)::connect(f, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        fillers.push_back(f);
    }
    std::this_thread::sleep_for(milliseconds(100));  // let the fillers queue up

    Client client;
    client.start("127.0.0.1", ntohs(addr.sin_port), "/ws");
    std::this_thread::sleep_for(milliseconds(200));  // worker is inside connect
    EXPECT_FALSE(client.is_connected());

    const auto t0 = Clock::now();
    client.stop();
    const auto took = std::chrono::duration_cast<milliseconds>(Clock::now() - t0);
    EXPECT_LT(took.count(), 1000);
    for (sock_t f : fillers) close_sock(f);
    close_sock(ls);
}
#endif

TEST(WsClient, StopReturnsPromptlyWhenServerNeverAnswers) {
    // TCP accepted (backlog) but no 101: the handshake read must not pin
    // the worker, so stop() still returns quickly.
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client client;
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client(kWait, /*send_handshake=*/false));

    const auto t0 = Clock::now();
    client.stop();
    const auto took = std::chrono::duration_cast<milliseconds>(Clock::now() - t0);
    EXPECT_LT(took.count(), 1000);
    EXPECT_FALSE(client.is_connected());
}

TEST(WsClient, StopReturnsPromptlyWhenPeerStopsReading) {
    // The peer keeps the TCP connection open but never reads (tool paused in
    // a debugger): once the socket buffers fill, sending must not pin the
    // worker, or stop() would block forever in join().
    LoopbackServer srv;
    ASSERT_TRUE(srv.ok());
    Client client;
    client.start("127.0.0.1", srv.port(), "/ws");
    ASSERT_TRUE(srv.accept_client());
    ASSERT_TRUE(wait_until([&] { return client.is_connected(); }));
    const std::string chunk(64 * 1024, 'x');
    for (int i = 0; i < 512; ++i) client.send_text(chunk);  // 32 MiB > any socket buffer
    ASSERT_TRUE(srv.read_frame().ok);  // the worker has taken the queue and is sending

    const auto t0 = Clock::now();
    client.stop();
    const auto took = std::chrono::duration_cast<milliseconds>(Clock::now() - t0);
    EXPECT_LT(took.count(), 1000);
    EXPECT_FALSE(client.is_connected());
}
