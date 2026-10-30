#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

#include "ergo/log/log.h"

using namespace ergo::log;

namespace {

/// Records every line it receives (message copied).
class CaptureSink : public Sink {
public:
    struct Line {
        Level       level;
        uint64_t    frame;
        int64_t     ts_us;
        uint64_t    tid;
        std::string msg;
    };

    void on_record(const Record& r) override {
        std::lock_guard<std::mutex> g(mtx_);
        lines_.push_back({r.level, r.frame, r.ts_us, r.tid, std::string(r.msg)});
    }

    std::vector<Line> lines() {
        std::lock_guard<std::mutex> g(mtx_);
        return lines_;
    }

private:
    std::mutex        mtx_;
    std::vector<Line> lines_;
};

/// Capture stdout into a temp file (same approach as test_log.cpp).
class StdoutCapture {
public:
    StdoutCapture() {
        std::fflush(stdout);
        old_ = dup_fd(stdout);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "ergo_log_sink_out_%p.txt", static_cast<void*>(this));
        path_ = buf;
        std::freopen(path_.c_str(), "w+", stdout);
    }
    ~StdoutCapture() {
        std::fflush(stdout);
        restore_fd(old_, stdout);
        std::remove(path_.c_str());
    }

    std::string text() {
        std::fflush(stdout);
        std::FILE* f = std::fopen(path_.c_str(), "rb");
        if (!f) return {};
        std::string out;
        char ch[256];
        while (auto n = std::fread(ch, 1, sizeof(ch), f)) out.append(ch, n);
        std::fclose(f);
        return out;
    }

private:
#ifdef _MSC_VER
    int  dup_fd(std::FILE* f)             { return _dup(_fileno(f)); }
    void restore_fd(int fd, std::FILE* f) { _dup2(fd, _fileno(f)); _close(fd); }
#else
    int  dup_fd(std::FILE* f)             { return ::dup(::fileno(f)); }
    void restore_fd(int fd, std::FILE* f) { ::dup2(fd, ::fileno(f)); ::close(fd); }
#endif

    int         old_ = -1;
    std::string path_;
};

bool contains(const std::string& s, const char* needle) {
    return s.find(needle) != std::string::npos;
}

/// Console off (keeps the test output clean), defaults restored after.
struct SinkTest : public ::testing::Test {
    void SetUp() override {
        set_level(Level::Debug);
        set_frame_provider({});
        set_time_provider({});
        remove_sink(&console_sink());
    }
    void TearDown() override {
        set_level(Level::Info);
        set_frame_provider({});
        set_time_provider({});
        add_sink(&console_sink());
    }
};

} // namespace

// -------------------------------------------------------------------------

TEST_F(SinkTest, CaptureSinkReceivesLevelFrameAndMessage) {
    CaptureSink cap;
    add_sink(&cap);
    set_frame_provider([]() -> uint64_t { return 4242; });
    ERGO_LOG_WARN("hello %d", 7);
    remove_sink(&cap);

    const auto lines = cap.lines();
    ASSERT_EQ(lines.size(), static_cast<size_t>(1));
    EXPECT_EQ(lines[0].level, Level::Warn);
    EXPECT_EQ(lines[0].frame, static_cast<uint64_t>(4242));
    EXPECT_EQ(lines[0].msg, std::string("hello 7"));   // payload only, no prefix
    const uint64_t me = std::hash<std::thread::id>{}(std::this_thread::get_id());
    EXPECT_EQ(lines[0].tid, me);
}

TEST_F(SinkTest, LongMessageReachesSinkWhole) {
    CaptureSink cap;
    add_sink(&cap);
    const std::string big(2000, 'y');
    ERGO_LOG_INFO("%s!", big.c_str());
    remove_sink(&cap);
    const auto lines = cap.lines();
    ASSERT_EQ(lines.size(), static_cast<size_t>(1));
    EXPECT_EQ(lines[0].msg, big + "!");
}

TEST_F(SinkTest, LevelFilterAppliesBeforeSinks) {
    CaptureSink cap;
    add_sink(&cap);
    set_level(Level::Warn);
    ERGO_LOG_INFO("filtered");
    ERGO_LOG_DEBUG("filtered");
    ERGO_LOG_ERROR("kept");
    remove_sink(&cap);

    const auto lines = cap.lines();
    ASSERT_EQ(lines.size(), static_cast<size_t>(1));
    EXPECT_EQ(lines[0].level, Level::Error);
    EXPECT_EQ(lines[0].msg, std::string("kept"));
}

TEST_F(SinkTest, RemovingConsoleSinkSilencesStdout) {
    CaptureSink cap;
    add_sink(&cap);
    {
        StdoutCapture out;
        ERGO_LOG_INFO("quiet line");               // console was removed in SetUp
        EXPECT_FALSE(contains(out.text(), "quiet line"));
    }
    {
        StdoutCapture out;
        add_sink(&console_sink());
        ERGO_LOG_INFO("loud line");
        EXPECT_TRUE(contains(out.text(), "[F00000000][INF] loud line"));
        remove_sink(&console_sink());
    }
    remove_sink(&cap);
    EXPECT_EQ(cap.lines().size(), static_cast<size_t>(2));   // the sink saw both
}

TEST_F(SinkTest, AddingTwiceIsANoOp) {
    CaptureSink cap;
    add_sink(&cap);
    add_sink(&cap);
    add_sink(nullptr);
    ERGO_LOG_INFO("once");
    remove_sink(&cap);
    remove_sink(&cap);   // not installed any more: no-op
    ERGO_LOG_INFO("after removal");
    EXPECT_EQ(cap.lines().size(), static_cast<size_t>(1));
}

namespace {
/// Logs from inside on_record: must be dropped, not recurse or deadlock.
class NestingSink : public Sink {
public:
    std::atomic<int> calls{0};
    void on_record(const Record&) override {
        ++calls;
        ERGO_LOG_ERROR("nested from a sink");
    }
};
} // namespace

TEST_F(SinkTest, NestedLogInsideSinkIsDropped) {
    NestingSink nest;
    CaptureSink cap;
    add_sink(&nest);
    add_sink(&cap);
    ERGO_LOG_INFO("outer");
    remove_sink(&nest);
    remove_sink(&cap);

    EXPECT_EQ(nest.calls.load(), 1);
    const auto lines = cap.lines();
    ASSERT_EQ(lines.size(), static_cast<size_t>(1));
    EXPECT_EQ(lines[0].msg, std::string("outer"));

    // The guard is per call: logging works again afterwards.
    CaptureSink after;
    add_sink(&after);
    ERGO_LOG_INFO("again");
    remove_sink(&after);
    EXPECT_EQ(after.lines().size(), static_cast<size_t>(1));
}

namespace {
/// Removes itself on its first record (remove_sink from inside a sink).
class SelfRemovingSink : public Sink {
public:
    std::atomic<int> calls{0};
    void on_record(const Record&) override {
        ++calls;
        remove_sink(this);
    }
};
} // namespace

TEST_F(SinkTest, SinkCanRemoveItselfWithoutDeadlock) {
    SelfRemovingSink self;
    add_sink(&self);
    ERGO_LOG_INFO("first");
    ERGO_LOG_INFO("second");
    EXPECT_EQ(self.calls.load(), 1);
}

TEST_F(SinkTest, TimeProviderIsUsed) {
    CaptureSink cap;
    add_sink(&cap);
    set_time_provider([]() -> int64_t { return 123456789; });
    ERGO_LOG_INFO("timed");
    set_time_provider([]() -> int64_t { throw 1; });
    ERGO_LOG_INFO("throwing provider");
    set_time_provider({});
    ERGO_LOG_INFO("default a");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ERGO_LOG_INFO("default b");
    remove_sink(&cap);

    const auto lines = cap.lines();
    ASSERT_EQ(lines.size(), static_cast<size_t>(4));
    EXPECT_EQ(lines[0].ts_us, static_cast<int64_t>(123456789));
    EXPECT_EQ(lines[1].ts_us, static_cast<int64_t>(0));
    // Default: steady_clock µs since first use — non-negative, monotonic.
    EXPECT_GE(lines[2].ts_us, static_cast<int64_t>(0));
    EXPECT_GE(lines[3].ts_us - lines[2].ts_us, static_cast<int64_t>(2000));
}

namespace {
/// Slow sink that tracks whether a call is in progress.
class SlowSink : public Sink {
public:
    std::atomic<bool> entered{false};
    std::atomic<bool> inside{false};
    void on_record(const Record&) override {
        inside  = true;
        entered = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        inside  = false;
    }
};
} // namespace

TEST_F(SinkTest, RemoveSinkWaitsForCallsInProgress) {
    SlowSink slow;
    add_sink(&slow);
    std::thread logger([] { ERGO_LOG_INFO("slow"); });
    while (!slow.entered.load()) std::this_thread::yield();
    remove_sink(&slow);
    // remove_sink returned: the logger thread must have left on_record.
    EXPECT_FALSE(slow.inside.load());
    logger.join();
}

namespace {
/// Uninstalls itself on its first record, then stays inside on_record().
class SlowSelfRemovingSink : public Sink {
public:
    std::atomic<bool> entered{false};
    std::atomic<int>  inside{0};
    void on_record(const Record&) override {
        ++inside;
        remove_sink(this);  // from inside a sink: uninstalls, does not wait
        entered = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        --inside;
    }
};
} // namespace

TEST_F(SinkTest, RemoveSinkWaitsAfterSelfRemoval) {
    SlowSelfRemovingSink self;
    add_sink(&self);
    std::thread logger([] { ERGO_LOG_INFO("self"); });
    while (!self.entered.load()) std::this_thread::yield();
    // Already uninstalled by the sink itself, but the owner's remove_sink
    // must still wait for the logger thread before `self` is destroyed.
    remove_sink(&self);
    EXPECT_EQ(self.inside.load(), 0);
    logger.join();
}

namespace {
/// Fails the test if it is called after its remove_sink() returned.
class TrackedSink : public Sink {
public:
    std::atomic<bool> removed{false};
    std::atomic<int>  late_calls{0};
    std::atomic<int>  calls{0};
    void on_record(const Record&) override {
        ++calls;
        if (removed.load()) ++late_calls;
    }
};
} // namespace

TEST_F(SinkTest, ConcurrentAddRemoveWhileLogging) {
    std::atomic<bool> stop{false};
    std::vector<std::thread> loggers;
    for (int t = 0; t < 2; ++t) {
        loggers.emplace_back([&stop, t] {
            int i = 0;
            while (!stop.load()) ERGO_LOG_INFO("t=%d i=%d", t, i++);
        });
    }

    static constexpr int kThreads = 4;
    static constexpr int kRounds  = 200;
    std::atomic<int> late{0};
    std::vector<std::thread> mutators;
    for (int t = 0; t < kThreads; ++t) {
        mutators.emplace_back([&late] {
            for (int r = 0; r < kRounds; ++r) {
                TrackedSink sink;
                add_sink(&sink);
                std::this_thread::yield();
                remove_sink(&sink);
                sink.removed = true;
                // Give a racing dispatch a chance to (wrongly) call it.
                std::this_thread::yield();
                late += sink.late_calls.load();
            }   // sink destroyed right after remove_sink: must be safe
        });
    }
    for (auto& m : mutators) m.join();
    stop = true;
    for (auto& l : loggers) l.join();

    EXPECT_EQ(late.load(), 0);

    // The list is intact afterwards: a fresh sink still receives lines.
    CaptureSink cap;
    add_sink(&cap);
    ERGO_LOG_INFO("still works");
    remove_sink(&cap);
    EXPECT_EQ(cap.lines().size(), static_cast<size_t>(1));
}
