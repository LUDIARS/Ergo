#include "gtest/gtest.h"

#include <cstdio>
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

/// Capture stdout / stderr into temp files, read back the contents.
/// Small ad-hoc helper; good enough for single-threaded line checks.
class StdCapture {
public:
    StdCapture() {
        std::fflush(stdout);
        std::fflush(stderr);
        old_stdout_ = dup_fd(stdout);
        old_stderr_ = dup_fd(stderr);
        out_path_ = tmp_path("log_out");
        err_path_ = tmp_path("log_err");
        std::freopen(out_path_.c_str(), "w+", stdout);
        std::freopen(err_path_.c_str(), "w+", stderr);
    }

    ~StdCapture() {
        std::fflush(stdout);
        std::fflush(stderr);
        restore_fd(old_stdout_, stdout);
        restore_fd(old_stderr_, stderr);
        std::remove(out_path_.c_str());
        std::remove(err_path_.c_str());
    }

    std::string stdout_text() { std::fflush(stdout); return slurp(out_path_); }
    std::string stderr_text() { std::fflush(stderr); return slurp(err_path_); }

private:
#ifdef _MSC_VER
    int  dup_fd(std::FILE* f)                { return _dup(_fileno(f)); }
    void restore_fd(int fd, std::FILE* f)    { _dup2(fd, _fileno(f)); _close(fd); }
#else
    int  dup_fd(std::FILE* f)                { return ::dup(::fileno(f)); }
    void restore_fd(int fd, std::FILE* f)    { ::dup2(fd, ::fileno(f)); ::close(fd); }
#endif

    static std::string tmp_path(const char* tag) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "ergo_log_%s_%p.txt", tag, (void*)&tag);
        return buf;
    }
    static std::string slurp(const std::string& p) {
        std::FILE* f = std::fopen(p.c_str(), "rb");
        if (!f) return {};
        std::string out;
        char ch[256];
        while (auto n = std::fread(ch, 1, sizeof(ch), f)) out.append(ch, n);
        std::fclose(f);
        return out;
    }

    int old_stdout_ = -1;
    int old_stderr_ = -1;
    std::string out_path_, err_path_;
};

bool contains(const std::string& s, const char* needle) {
    return s.find(needle) != std::string::npos;
}

} // namespace

// -------------------------------------------------------------------------

TEST(Log, DefaultLevelIsInfo) {
    set_frame_provider({});
    set_level(Level::Info);
    EXPECT_EQ(level(), Level::Info);
}

TEST(Log, ErrorAndWarnGoToStderr) {
    set_frame_provider({});
    set_level(Level::Debug);
    StdCapture cap;
    ERGO_LOG_ERROR("boom %d", 1);
    ERGO_LOG_WARN ("watch %d", 2);
    const std::string e = cap.stderr_text();
    EXPECT_TRUE(contains(e, "boom 1"));
    EXPECT_TRUE(contains(e, "watch 2"));
    EXPECT_TRUE(contains(e, "[ERR]"));
    EXPECT_TRUE(contains(e, "[WRN]"));
}

TEST(Log, InfoAndDebugGoToStdout) {
    set_frame_provider({});
    set_level(Level::Debug);
    StdCapture cap;
    ERGO_LOG_INFO ("hello %s", "world");
    ERGO_LOG_DEBUG("ping %d",  42);
    const std::string o = cap.stdout_text();
    EXPECT_TRUE(contains(o, "hello world"));
    EXPECT_TRUE(contains(o, "ping 42"));
    EXPECT_TRUE(contains(o, "[INF]"));
    EXPECT_TRUE(contains(o, "[DBG]"));
}

TEST(Log, LevelFilterDropsLowerPriority) {
    set_frame_provider({});
    set_level(Level::Warn);
    StdCapture cap;
    ERGO_LOG_INFO ("should not appear");
    ERGO_LOG_DEBUG("should not appear");
    ERGO_LOG_WARN ("appears");
    const std::string o = cap.stdout_text();
    const std::string e = cap.stderr_text();
    EXPECT_FALSE(contains(o, "should not appear"));
    EXPECT_FALSE(contains(e, "should not appear"));
    EXPECT_TRUE (contains(e, "appears"));
}

TEST(Log, FrameProviderAppearsInPrefix) {
    set_level(Level::Debug);
    set_frame_provider([]() -> uint64_t { return 1234567ull; });
    StdCapture cap;
    ERGO_LOG_INFO("with frame");
    const std::string o = cap.stdout_text();
    EXPECT_TRUE(contains(o, "[F01234567]"));
    EXPECT_TRUE(contains(o, "with frame"));
    set_frame_provider({});
}

TEST(Log, FrameProviderResetToZero) {
    set_level(Level::Debug);
    set_frame_provider({});
    StdCapture cap;
    ERGO_LOG_INFO("plain");
    const std::string o = cap.stdout_text();
    EXPECT_TRUE(contains(o, "[F00000000]"));
}

TEST(Log, FormattingWithLongPayload) {
    set_level(Level::Debug);
    set_frame_provider({});
    StdCapture cap;
    // Exceed stack buffer on purpose (>512 chars).
    std::string big(1024, 'x');
    ERGO_LOG_INFO("%s", big.c_str());
    const std::string o = cap.stdout_text();
    EXPECT_TRUE(contains(o, "xxxxxxxxxx"));
    EXPECT_GT(o.size(), big.size());
}

TEST(Log, ThreadSafeWriteNoCrash) {
    set_level(Level::Debug);
    set_frame_provider({});
    StdCapture cap;
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t) {
        ts.emplace_back([t]{
            for (int i = 0; i < 50; ++i) {
                ERGO_LOG_INFO("t=%d i=%d", t, i);
            }
        });
    }
    for (auto& th : ts) th.join();
    const std::string o = cap.stdout_text();
    // 200 lines expected.
    int lines = 0;
    for (char c : o) if (c == '\n') ++lines;
    EXPECT_GE(lines, 200);
}
