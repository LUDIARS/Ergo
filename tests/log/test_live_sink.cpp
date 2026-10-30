// ergo_log_live — LiveSink: frame encoding, the bounded queue and its drop
// count, and installation through ergo_log, all without a socket. The last
// tests start the worker against an in-process loopback server (127.0.0.1,
// port 0) that answers the upgrade and then never reads, to pin down the
// back-pressure: a stalled tool costs counted drops in LiveSink's own queue
// and never blocks stop(). spec/feature/tool/log.md has the wire protocol
// the tools/ergo `log` plugin tests against.

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "ergo/common/json_min.h"
#include "ergo/log/live_sink.h"
#include "ergo/log/log.h"

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
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using sock_t = int;
  static const sock_t kBadSock = -1;
  static void close_sock(sock_t s) { ::close(s); }
  using socklen_type = socklen_t;
#endif

using namespace ergo::log;
namespace jsonm = ergo::common::jsonm;
using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

namespace {

#ifdef _WIN32
struct WsaInit {
    WsaInit()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaInit() { WSACleanup(); }
} g_wsa_init;
#endif

template <class Pred>
bool wait_until(Pred pred, milliseconds timeout = milliseconds(3000)) {
    const auto deadline = Clock::now() + timeout;
    while (Clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(milliseconds(2));
    }
    return pred();
}

bool readable(sock_t s, milliseconds timeout) {
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(s, &rf);
    timeval tv{};
    tv.tv_sec  = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    return ::select(static_cast<int>(s + 1), &rf, nullptr, nullptr, &tv) > 0;
}

/// A tool that accepts the engine, answers the WebSocket upgrade and then
/// never reads again (paused in a debugger, SIGSTOPped).
class StalledServer {
public:
    StalledServer() {
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
    ~StalledServer() {
        if (peer_ != kBadSock) close_sock(peer_);
        if (listen_ != kBadSock) close_sock(listen_);
    }

    bool     ok()   const { return ok_ && port_ != 0; }
    uint16_t port() const { return port_; }

    bool accept_and_upgrade() {
        if (!readable(listen_, milliseconds(3000))) return false;
        peer_ = ::accept(listen_, nullptr, nullptr);
        if (peer_ == kBadSock) return false;
        std::string req;
        char buf[1024];
        while (req.find("\r\n\r\n") == std::string::npos) {
            if (!readable(peer_, milliseconds(3000))) return false;
            const int n = ::recv(peer_, buf, sizeof(buf), 0);
            if (n <= 0) return false;
            req.append(buf, static_cast<std::size_t>(n));
        }
        const std::string resp = "HTTP/1.1 101 Switching Protocols\r\n"
                                 "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                                 "Sec-WebSocket-Accept: test\r\n\r\n";
        return ::send(peer_, resp.data(), static_cast<int>(resp.size()), 0)
               == static_cast<int>(resp.size());
    }

private:
    sock_t   listen_ = kBadSock;
    sock_t   peer_   = kBadSock;
    uint16_t port_   = 0;
    bool     ok_     = false;
};

Record make_record(Level lvl, uint64_t frame, const std::string& msg) {
    return Record{lvl, frame, static_cast<int64_t>(frame * 10), 77, msg};
}

std::string str(const jsonm::JsonValue& obj, const char* key) {
    const jsonm::JsonValue* v = obj.find(key);
    return v ? v->as_string() : std::string("<missing>");
}

double num(const jsonm::JsonValue& obj, const char* key) {
    const jsonm::JsonValue* v = obj.find(key);
    return v ? v->as_number(-1.0) : -1.0;
}

/// Parse a `lines` frame; `lines` receives its array.
bool parse_lines(const std::string& frame, jsonm::JsonValue& root, std::vector<jsonm::JsonValue>& lines) {
    if (!jsonm::parse(frame, root) || !root.is_object()) return false;
    const jsonm::JsonValue* arr = root.find("lines");
    if (!arr || !arr->is_array() || !arr->a) return false;
    lines = *arr->a;
    return true;
}

} // namespace

// -------------------------------------------------------------------------

TEST(LiveSink, HelloFrameShape) {
    jsonm::JsonValue root;
    ASSERT_TRUE(jsonm::parse(encode_hello_frame("my game"), root));
    EXPECT_EQ(str(root, "op"),   std::string("hello"));
    EXPECT_EQ(str(root, "role"), std::string("engine"));
    EXPECT_EQ(str(root, "app"),  std::string("my game"));
}

TEST(LiveSink, LinesFrameShape) {
    std::vector<OwnedRecord> recs;
    recs.push_back({Level::Error, 12, 3456, 99, "boom"});
    recs.push_back({Level::Debug, 13, -1,   100, ""});
    const std::string frame = encode_lines_frame("app", recs, 5);

    jsonm::JsonValue root;
    std::vector<jsonm::JsonValue> lines;
    ASSERT_TRUE(parse_lines(frame, root, lines));
    EXPECT_EQ(str(root, "op"),  std::string("lines"));
    EXPECT_EQ(str(root, "app"), std::string("app"));
    EXPECT_EQ(num(root, "dropped"), 5.0);
    ASSERT_EQ(lines.size(), static_cast<size_t>(2));

    EXPECT_EQ(str(lines[0], "lvl"), std::string("ERR"));
    EXPECT_EQ(num(lines[0], "frame"), 12.0);
    EXPECT_EQ(num(lines[0], "ts_us"), 3456.0);
    EXPECT_EQ(num(lines[0], "tid"),   99.0);
    EXPECT_EQ(str(lines[0], "msg"), std::string("boom"));

    EXPECT_EQ(str(lines[1], "lvl"), std::string("DBG"));
    EXPECT_EQ(num(lines[1], "ts_us"), -1.0);
    EXPECT_EQ(str(lines[1], "msg"), std::string(""));

    // Empty batch: still a valid frame (carries only a drop count).
    jsonm::JsonValue empty_root;
    std::vector<jsonm::JsonValue> none;
    ASSERT_TRUE(parse_lines(encode_lines_frame("app", {}, 3), empty_root, none));
    EXPECT_EQ(none.size(), static_cast<size_t>(0));
    EXPECT_EQ(num(empty_root, "dropped"), 3.0);
}

TEST(LiveSink, LinesFrameEscapesMessages) {
    const std::string tricky = "quote\" back\\slash\nnew\tline \x01 ctl \xE3\x81\x82";
    std::vector<OwnedRecord> recs;
    recs.push_back({Level::Info, 1, 2, 3, tricky});
    const std::string frame = encode_lines_frame("a\"pp", recs, 0);

    // Escaped on the wire...
    EXPECT_TRUE(frame.find("quote\\\"") != std::string::npos);
    EXPECT_TRUE(frame.find("back\\\\slash") != std::string::npos);
    EXPECT_TRUE(frame.find("\\nnew\\tline") != std::string::npos);
    EXPECT_TRUE(frame.find("\\u0001") != std::string::npos);
    EXPECT_TRUE(frame.find('\n') == std::string::npos);   // one line of JSON

    // ...and back to the exact bytes.
    jsonm::JsonValue root;
    std::vector<jsonm::JsonValue> lines;
    ASSERT_TRUE(parse_lines(frame, root, lines));
    EXPECT_EQ(str(root, "app"), std::string("a\"pp"));
    ASSERT_EQ(lines.size(), static_cast<size_t>(1));
    EXPECT_EQ(str(lines[0], "msg"), tricky);
}

TEST(LiveSink, QueueIsBoundedAndCountsDrops) {
    LiveSink live(8);
    for (int i = 0; i < 20; ++i) {
        const std::string msg = "#" + std::to_string(i);
        live.on_record(make_record(Level::Info, static_cast<uint64_t>(i), msg));
    }
    EXPECT_EQ(live.pending(), static_cast<size_t>(8));
    EXPECT_EQ(live.dropped(), static_cast<uint64_t>(12));

    // Oldest first; the first frame carries the drops since the last one.
    jsonm::JsonValue root;
    std::vector<jsonm::JsonValue> lines;
    ASSERT_TRUE(parse_lines(live.take_frame(5), root, lines));
    EXPECT_EQ(num(root, "dropped"), 12.0);
    EXPECT_EQ(str(root, "app"), std::string("app"));   // default name before start()
    ASSERT_EQ(lines.size(), static_cast<size_t>(5));
    EXPECT_EQ(str(lines[0], "msg"), std::string("#12"));
    EXPECT_EQ(num(lines[0], "frame"), 12.0);
    EXPECT_EQ(num(lines[0], "ts_us"), 120.0);
    EXPECT_EQ(str(lines[4], "msg"), std::string("#16"));
    EXPECT_EQ(live.pending(), static_cast<size_t>(3));

    ASSERT_TRUE(parse_lines(live.take_frame(), root, lines));
    EXPECT_EQ(num(root, "dropped"), 0.0);   // already reported
    ASSERT_EQ(lines.size(), static_cast<size_t>(3));
    EXPECT_EQ(str(lines[2], "msg"), std::string("#19"));

    EXPECT_EQ(live.take_frame(), std::string());   // nothing left to report
    EXPECT_EQ(live.dropped(), static_cast<uint64_t>(12));   // lifetime total stays
}

TEST(LiveSink, DefaultCapacityAndBatchSize) {
    LiveSink live;
    const size_t total = LiveSink::kQueueCapacity + 904;
    for (size_t i = 0; i < total; ++i) {
        live.on_record(make_record(Level::Debug, i, "#" + std::to_string(i)));
    }
    EXPECT_EQ(live.pending(), LiveSink::kQueueCapacity);
    EXPECT_EQ(live.dropped(), static_cast<uint64_t>(904));

    jsonm::JsonValue root;
    std::vector<jsonm::JsonValue> lines;
    ASSERT_TRUE(parse_lines(live.take_frame(), root, lines));
    EXPECT_EQ(lines.size(), LiveSink::kMaxBatchLines);
    EXPECT_EQ(str(lines[0], "msg"), std::string("#904"));
    EXPECT_EQ(num(root, "dropped"), 904.0);
}

TEST(LiveSink, LongMessageIsClippedOnUtf8Boundary) {
    LiveSink live;
    // 'a' x (max-1) followed by a 3-byte character straddling the limit.
    const std::string msg = std::string(LiveSink::kMaxMessageBytes - 1, 'a') + "\xE3\x81\x82" + "tail";
    live.on_record(make_record(Level::Info, 1, msg));

    jsonm::JsonValue root;
    std::vector<jsonm::JsonValue> lines;
    ASSERT_TRUE(parse_lines(live.take_frame(), root, lines));
    ASSERT_EQ(lines.size(), static_cast<size_t>(1));
    const std::string got = str(lines[0], "msg");
    EXPECT_EQ(got, std::string(LiveSink::kMaxMessageBytes - 1, 'a') + "\xE2\x80\xA6");

    // Short messages pass through untouched.
    live.on_record(make_record(Level::Info, 2, "short"));
    ASSERT_TRUE(parse_lines(live.take_frame(), root, lines));
    EXPECT_EQ(str(lines[0], "msg"), std::string("short"));
}

TEST(LiveSink, ReceivesRecordsThroughErgoLog) {
    remove_sink(&console_sink());
    set_level(Level::Debug);
    set_frame_provider([]() -> uint64_t { return 31; });
    {
        LiveSink live;
        add_sink(&live);
        ERGO_LOG_INFO("hi %s", "there");
        ERGO_LOG_DEBUG("dbg");
        EXPECT_EQ(live.pending(), static_cast<size_t>(2));

        jsonm::JsonValue root;
        std::vector<jsonm::JsonValue> lines;
        ASSERT_TRUE(parse_lines(live.take_frame(), root, lines));
        ASSERT_EQ(lines.size(), static_cast<size_t>(2));
        EXPECT_EQ(str(lines[0], "lvl"), std::string("INF"));
        EXPECT_EQ(str(lines[0], "msg"), std::string("hi there"));
        EXPECT_EQ(num(lines[0], "frame"), 31.0);
        // `live` is still installed here: its destructor uninstalls it.
    }
    ERGO_LOG_INFO("after the sink is gone");   // must not touch the dead sink

    set_frame_provider({});
    set_level(Level::Info);
    add_sink(&console_sink());
}

TEST(LiveSink, StartRejectsBadArguments) {
    LiveSink live;
    EXPECT_FALSE(live.start("", 5170, "app"));
    EXPECT_FALSE(live.start("127.0.0.1", 0, "app"));
    EXPECT_FALSE(live.start("127.0.0.1", 5170, ""));
    EXPECT_FALSE(live.is_connected());
    live.stop();   // never started: no-op
    live.stop();
}

// -------------------------------------------------------------------------
// Against a tool that stops reading

TEST(LiveSink, QueueStaysTheBoundWhileTheToolStopsReading) {
    StalledServer srv;
    ASSERT_TRUE(srv.ok());
    constexpr std::size_t kCap = 256;
    LiveSink live(kCap);
    ASSERT_TRUE(live.start("127.0.0.1", srv.port(), "stall"));
    ASSERT_TRUE(srv.accept_and_upgrade());
    ASSERT_TRUE(wait_until([&] { return live.is_connected(); }));

    // Feed full-size records until the socket backs up. From then on the
    // worker must leave records in LiveSink's own queue (not move them into
    // the transport's unbounded one), so the queue stays full.
    const std::string big(LiveSink::kMaxMessageBytes, 'x');
    bool saturated = false;
    for (int round = 0; round < 60 && !saturated; ++round) {
        for (std::size_t i = 0; i < kCap; ++i) live.on_record(Record{Level::Info, 0, 0, 1, big});
        std::this_thread::sleep_for(milliseconds(200));
        saturated = live.pending() == kCap;
    }
    ASSERT_TRUE(saturated);  // else the worker kept draining into the socket
    EXPECT_TRUE(live.is_connected());

    // Overflow is dropped and counted here, while still connected.
    const uint64_t before = live.dropped();
    for (std::size_t i = 0; i < 2 * kCap; ++i) live.on_record(Record{Level::Info, 0, 0, 1, big});
    EXPECT_EQ(live.pending(), kCap);
    EXPECT_GT(live.dropped(), before);

    const auto t0 = Clock::now();
    live.stop();
    const auto took = std::chrono::duration_cast<milliseconds>(Clock::now() - t0);
    EXPECT_LT(took.count(), 1000);
    EXPECT_EQ(live.pending(), kCap);  // still queued for the next start()
}

TEST(LiveSink, StopIsPromptWhileThreadsKeepLogging) {
    StalledServer srv;
    ASSERT_TRUE(srv.ok());
    LiveSink live;
    ASSERT_TRUE(live.start("127.0.0.1", srv.port(), "busy"));
    ASSERT_TRUE(srv.accept_and_upgrade());
    ASSERT_TRUE(wait_until([&] { return live.is_connected(); }));

    // Producers that never let the queue run empty must not keep the
    // batching worker (and so stop()) busy.
    std::atomic<bool> quit{false};
    std::vector<std::thread> loggers;
    for (int t = 0; t < 3; ++t) {
        loggers.emplace_back([&live, &quit] {
            while (!quit.load()) live.on_record(Record{Level::Info, 0, 0, 2, "line"});
        });
    }
    std::this_thread::sleep_for(milliseconds(200));

    const auto t0 = Clock::now();
    live.stop();
    const auto took = std::chrono::duration_cast<milliseconds>(Clock::now() - t0);
    quit = true;
    for (std::thread& t : loggers) t.join();
    EXPECT_LT(took.count(), 1000);
    EXPECT_FALSE(live.is_connected());
}
