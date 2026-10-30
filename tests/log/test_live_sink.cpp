// ergo_log_live — LiveSink without a socket: frame encoding, the bounded
// queue and its drop count, and installation through ergo_log. The worker
// and the socket are never started here (see spec/feature/tool/log.md for
// the wire protocol the tools/ergo `log` plugin tests against).

#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "ergo/common/json_min.h"
#include "ergo/log/live_sink.h"
#include "ergo/log/log.h"

using namespace ergo::log;
namespace jsonm = ergo::common::jsonm;

namespace {

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
