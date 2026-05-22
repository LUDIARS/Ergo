#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <thread>

#include "ergo/profile/profile.h"

using namespace ergo::profile;

namespace {

void busy_us(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

/// A minimal app-side MarkerSink implementation, as a host would write.
class CountingSink : public MarkerSink {
public:
    int events = 0;
    void on_event(const Event&) override { ++events; }
};

} // namespace

TEST(Profile, StartsEmptyAfterBeginSession) {
    begin_session();
    EXPECT_EQ(event_count(), 0u);
}

TEST(Profile, ScopeRecordsCompleteEvent) {
    begin_session();
    {
        ERGO_PROFILE_SCOPE("work");
        busy_us(200);
    }
    EXPECT_EQ(event_count(), 1u);
}

TEST(Profile, MarkRecordsInstant) {
    begin_session();
    ERGO_PROFILE_MARK("frame_start");
    EXPECT_EQ(event_count(), 1u);
}

TEST(Profile, CounterAndMemRecord) {
    begin_session();
    ERGO_PROFILE_COUNTER("draw_calls", 42);
    ERGO_PROFILE_MEM("memory.rss");
    EXPECT_EQ(event_count(), 2u);
}

TEST(Profile, ClearEmptiesEvents) {
    begin_session();
    ERGO_PROFILE_MARK("a");
    ERGO_PROFILE_MARK("b");
    EXPECT_EQ(event_count(), 2u);
    clear();
    EXPECT_EQ(event_count(), 0u);
}

TEST(Profile, DisabledStopsRecording) {
    begin_session();
    set_enabled(false);
    ERGO_PROFILE_MARK("ignored");
    EXPECT_EQ(event_count(), 0u);
    set_enabled(true);
    ERGO_PROFILE_MARK("kept");
    EXPECT_EQ(event_count(), 1u);
}

TEST(Profile, RecordsAcrossThreads) {
    begin_session();
    ERGO_PROFILE_MARK("main_thread");
    std::thread t([] {
        ERGO_PROFILE_THREAD("worker");
        ERGO_PROFILE_MARK("worker_thread");
    });
    t.join();
    EXPECT_EQ(event_count(), 2u);
}

TEST(Profile, ExportProducesChromeTraceJson) {
    begin_session();
    {
        ERGO_PROFILE_SCOPE("scope");
        busy_us(100);
    }
    ERGO_PROFILE_MARK("mark");
    const std::string json = export_chrome_trace();
    EXPECT_TRUE(json.find("\"traceEvents\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"ph\":\"X\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"ph\":\"i\"") != std::string::npos);
}

TEST(Profile, AspectForwardsResultAndRecords) {
    begin_session();
    const int r = aspect([] { return 7; }, "compute");
    EXPECT_EQ(r, 7);
    EXPECT_EQ(event_count(), 1u);
}

TEST(Profile, NowAdvances) {
    begin_session();
    const int64_t t0 = now_us();
    busy_us(500);
    const int64_t t1 = now_us();
    EXPECT_TRUE(t1 >= t0);
}

TEST(Profile, CustomSinkReceivesMarkers) {
    begin_session();
    CountingSink sink;
    set_sink(&sink);
    ERGO_PROFILE_MARK("a");
    ERGO_PROFILE_COUNTER("c", 1);
    {
        ERGO_PROFILE_SCOPE("s");  // recorded on scope exit, still inside
    }
    set_sink(nullptr);  // restore the built-in collector
    // instant + counter + complete all routed to the app sink
    EXPECT_EQ(sink.events, 3);
    // the built-in collector saw nothing while the custom sink was installed
    EXPECT_EQ(event_count(), 0u);
    // ...and recording returns to the collector after restore
    ERGO_PROFILE_MARK("back");
    EXPECT_EQ(event_count(), 1u);
}
