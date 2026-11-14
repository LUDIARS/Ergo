#include "fake_ws.h"
#include "plugins/log/lines_query.h"
#include "plugins/log/log_apps.h"
#include "plugins/log/log_plugin.h"

#include <gtest/gtest.h>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::log;
using namespace studio_test;

namespace {

LogLine line(const std::string& lvl, double frame, const std::string& msg) { return {lvl, frame, 0, 0, msg}; }

std::string lines_frame(const std::string& app, int n, const char* lvl = "INF") {
    std::string s = "{\"op\":\"lines\",\"app\":\"" + app + "\",\"dropped\":0,\"lines\":[";
    for (int i = 0; i < n; ++i) {
        if (i) s += ",";
        s += "{\"lvl\":\"" + std::string(lvl) + "\",\"frame\":" + std::to_string(i) +
             ",\"ts_us\":0,\"tid\":1,\"msg\":\"m" + std::to_string(i) + "\"}";
    }
    return s + "]}";
}

} // namespace

TEST(StudioLogProtocol, SanitizesLinesAndClipsUtf8) {
    JsonValue msg;
    ASSERT_TRUE(ergo::common::jsonm::parse(
        R"({"op":"lines","app":"g","dropped":2.7,"lines":[
            {"lvl":"ERR","frame":1,"ts_us":2,"tid":3,"msg":"ok"},
            {"lvl":"BAD","frame":1,"ts_us":2,"tid":3,"msg":"x"},
            {"lvl":"INF","frame":"1","ts_us":2,"tid":3,"msg":"x"},
            {"lvl":"DBG","frame":1,"ts_us":2,"tid":3}]})",
        msg));
    const auto body = sanitize_lines_msg(msg);
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(body->app, "g");
    EXPECT_EQ(body->dropped, 2u);
    ASSERT_EQ(body->lines.size(), 1u);
    EXPECT_EQ(body->lines[0].lvl, "ERR");

    // 8191 ASCII bytes + a 3-byte character crosses the limit inside the character.
    const std::string long_msg = std::string(8191, 'a') + "\xE3\x81\x82" + "tail";
    const std::string clipped  = clip_message(long_msg);
    EXPECT_EQ(clipped, std::string(8191, 'a') + "\xE2\x80\xA6");
    EXPECT_EQ(clip_message("short"), "short");
}

TEST(StudioLogProtocol, ParsesConsoleLines) {
    const auto a = parse_console_line("[F00000042][WRN] low memory\r\n");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->frame, 42u);
    EXPECT_EQ(a->lvl, "WRN");
    EXPECT_EQ(a->msg, "low memory");
    const auto b = parse_console_line("[F1][ERR]");
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(b->msg, "");
    EXPECT_FALSE(parse_console_line("[F][ERR] x").has_value());
    EXPECT_FALSE(parse_console_line("[F1][XXX] x").has_value());
    EXPECT_FALSE(parse_console_line("plain text").has_value());
    EXPECT_FALSE(parse_console_line("[F1][ERR]x").has_value());
}

TEST(StudioLogRing, KeepsNewestAndFilters) {
    LineRing ring(3);
    for (int i = 0; i < 5; ++i) ring.push(line(i % 2 ? "ERR" : "INF", i, i == 4 ? "Boom" : "m"));
    EXPECT_EQ(ring.size(), 3u);
    EXPECT_EQ(ring.last_seq(), 5u);
    EXPECT_EQ(ring.first_seq(), 3u);
    const auto all = ring.query();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all.front().seq, 3u);

    LineFilter f;
    f.since = 3;
    EXPECT_EQ(ring.query(f).size(), 2u);
    f       = {};
    f.q     = "BOOM";
    const auto hit = ring.query(f);   // keep the vector alive: EXPECT_EQ binds references
    ASSERT_EQ(hit.size(), 1u);
    EXPECT_EQ(hit[0].seq, 5u);
    f        = {};
    f.levels = std::set<std::string>{"ERR"};
    EXPECT_EQ(ring.query(f).size(), 1u);
    const auto newest = ring.query({}, 2);   // newest 2, oldest first
    EXPECT_EQ(newest.front().seq, 4u);
}

TEST(StudioLogApps, SuffixesLiveNamesAndTakesOverDeadOnes) {
    LogApps apps(10);
    EXPECT_EQ(apps.claim("game", 1), "game");
    EXPECT_EQ(apps.claim("game", 2), "game#2");
    EXPECT_EQ(apps.claim("  ", 3), "anonymous");
    apps.ingest("game", 0, {line("INF", 0, "a")});
    EXPECT_TRUE(apps.release("game", 1));
    EXPECT_FALSE(apps.release("game", 1));
    EXPECT_EQ(apps.claim("game", 4), "game");   // restarted engine continues its ring
    // Kept in a local: EXPECT_EQ binds its operands by reference, and a member
    // of the temporary optional dies before the comparison (ASan, clang).
    const auto game = apps.query("game");
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->last_seq, 1u);
}

TEST(StudioLogApps, EvictsOldestDisconnectedApp) {
    LogApps apps(4);
    for (uint64_t i = 1; i <= kMaxApps; ++i) apps.claim("a" + std::to_string(i), i);
    apps.release("a3", 3);
    apps.release("a1", 1);   // a3 left first: it is the least recently active
    apps.claim("new", 100);
    EXPECT_EQ(apps.size(), kMaxApps);
    EXPECT_FALSE(apps.has("a3"));
    EXPECT_TRUE(apps.has("a1"));
}

TEST(StudioLogQuery, ParsesParameters) {
    const auto ok = parse_lines_query({{"level", "err, wrn"}, {"since", "5"}, {"limit", "99999"}, {"q", "x"}});
    ASSERT_TRUE(std::holds_alternative<LinesQuery>(ok));
    const auto& q = std::get<LinesQuery>(ok);
    EXPECT_EQ(q.filter.levels->size(), 2u);
    EXPECT_EQ(*q.filter.since, 5.0);
    EXPECT_EQ(q.limit, kRingCapacity);
    const auto defaults = parse_lines_query({});  // a local: EXPECT_EQ binds by reference
    EXPECT_EQ(std::get<LinesQuery>(defaults).limit, kDefaultLinesLimit);
    EXPECT_TRUE(std::holds_alternative<std::string>(parse_lines_query({{"level", "ERR,NOPE"}})));
    EXPECT_TRUE(std::holds_alternative<std::string>(parse_lines_query({{"since", "abc"}})));
    EXPECT_TRUE(std::holds_alternative<std::string>(parse_lines_query({{"limit", "1.5"}})));
    EXPECT_TRUE(std::holds_alternative<std::string>(parse_lines_query({{"limit", "0"}})));
}

TEST(StudioLogPlugin, RelaysLinesAndServesBacklog) {
    LogPlugin p(quiet());
    Client engine = open(p, 1);
    engine.h.on_text(R"({"op":"hello","role":"engine","app":"game"})");
    engine.h.on_text(lines_frame("game", 3));

    Client ui = open(p, 2);
    ui.h.on_text(R"({"op":"hello","role":"ui"})");
    const JsonValue backlog = ui.conn->last("backlog");
    EXPECT_EQ(field(backlog, "app")->as_string(), "game");
    EXPECT_EQ(field(backlog, "lines")->a->size(), 3u);

    engine.h.on_text(lines_frame("game", 1, "ERR"));
    const JsonValue live = ui.conn->last("lines");
    EXPECT_EQ(field(live, "lines")->a->at(0).find("seq")->as_number(), 4.0);

    net::HttpRequest r = req("GET");
    EXPECT_EQ(p.handle(r, "/api/lines").status, 400);
    r.query = {{"app", "nope"}};
    EXPECT_EQ(p.handle(r, "/api/lines").status, 404);
    r.query = {{"app", "game"}, {"level", "ERR"}};
    JsonValue body;
    ASSERT_TRUE(ergo::common::jsonm::parse(p.handle(r, "/api/lines").body, body));
    EXPECT_EQ(field(body, "lines")->a->size(), 1u);
    EXPECT_EQ(field(body, "last_seq")->as_number(), 4.0);

    engine.h.on_close();
    const auto after = p.apps();
    EXPECT_FALSE(after[0].connected);
    EXPECT_FALSE(field(ui.conn->last("apps"), "apps")->a->at(0).find("connected")->as_bool());
    EXPECT_EQ(after[0].lines, 4u);   // the ring outlives the engine
}

TEST(StudioLogPlugin, LinesBeforeHelloRegisterTheEngine) {
    LogPlugin p(quiet());
    Client engine = open(p, 1);
    engine.h.on_text(lines_frame("late", 2));
    const auto apps = p.apps();
    ASSERT_EQ(apps.size(), 1u);
    EXPECT_EQ(apps[0].app, "late");
    EXPECT_EQ(apps[0].lines, 2u);
    EXPECT_EQ(field(p.snapshot(), "engines")->as_number(), 1.0);
}

TEST(StudioLogPlugin, RefusesEnginesBeyondTheLimit) {
    LogPlugin p(quiet());
    std::vector<Client> engines;
    for (uint64_t i = 1; i <= kMaxEngines + 1; ++i) {
        engines.push_back(open(p, i));
        engines.back().h.on_text(R"({"op":"hello","role":"engine","app":"g"})");
    }
    EXPECT_EQ(engines.back().conn->close_code(), 1013);
    EXPECT_EQ(field(p.snapshot(), "engines")->as_number(), static_cast<double>(kMaxEngines));
}
