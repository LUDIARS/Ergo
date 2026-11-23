// Remote play (spec/feature/tool/play.md): the profile's "play" entries, the WS
// messages, line splitting, the build / run state, and the play plugin's REST
// and WS with fake processes and a fake remote play (the video, input and ICE
// parts are Odeum's, tested there: odeum.remote_play).
#include "plugins/common/json_parse.h"
#include "plugins/common/workspace_profiles.h"
#include "plugins/play/play_message.h"
#include "plugins/play/play_plugin.h"
#include "plugins/play/play_target.h"
#include "plugins/play/target_runner.h"

#include "ergo/studio/platform/live_process.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::play;

namespace {

// The shim's macros may `return;`, so helpers that return a value check nothing
// themselves: a malformed fixture shows up as an empty result in the test.
JsonValue json(const std::string& text) {
    auto v = parse_json(text);
    return v ? *v : JsonValue{};
}

const char* kTargets = R"([{
  "name": "amicii", "label": "Amicii",
  "build": {"cmd": "node", "args": ["scripts/build.mjs"]},
  "run": {"cmd": "node", "args": ["scripts/run.mjs", "--x"]},
  "window": "Amicii", "fps": 24, "bitrate": 3000000,
  "buttons": [{"label": "決定", "key": "Enter"}, {"label": "上", "key": "Up"}]
}, {"name": "tool", "run": {"cmd": "tool.exe"}, "window": "Tool"}])";

std::vector<PlayTarget> parse_ok(const std::string& text) {
    std::vector<PlayTarget> out;
    std::string             err;
    if (!parse_play_targets(json(text), "p", "/root", out, err)) out.clear();
    return out;
}

/// The error, or "" when the text parsed (a test expecting an error then fails).
std::string parse_err(const std::string& text) {
    std::vector<PlayTarget> out;
    std::string             err;
    if (parse_play_targets(json(text), "p", "/root", out, err) || !out.empty()) return {};
    return err;
}

} // namespace

// ---- profile "play" ----------------------------------------------------------

TEST(PlayTarget, ReadsEveryField) {
    const auto list = parse_ok(kTargets);
    ASSERT_EQ(list.size(), 2u);
    const auto& a = list[0];
    EXPECT_EQ(a.name, "amicii");
    EXPECT_EQ(a.label, "Amicii");
    EXPECT_EQ(a.profile, "p");
    EXPECT_EQ(a.root, std::filesystem::path("/root"));
    ASSERT_TRUE(a.build.has_value());
    EXPECT_EQ(a.build->cmd, "node");
    EXPECT_EQ(a.build->timeout_sec, 3600);
    EXPECT_EQ(a.run.args, (std::vector<std::string>{"scripts/run.mjs", "--x"}));
    EXPECT_EQ(a.window, "Amicii");
    EXPECT_EQ(a.fps, 24);
    EXPECT_EQ(a.bitrate, 3000000);
    ASSERT_EQ(a.buttons.size(), 2u);
    EXPECT_EQ(a.buttons[1].key, "Up");
    const auto& t = list[1];
    EXPECT_EQ(t.label, "tool");
    EXPECT_FALSE(t.build.has_value());
    EXPECT_EQ(t.fps, 30);
    EXPECT_EQ(t.bitrate, 4000000);
}

TEST(PlayTarget, RejectsBadEntries) {
    EXPECT_NE(parse_err(R"({"name":"x"})").find("array"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"Bad Name","run":{"cmd":"a"},"window":"w"}])").find("name"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","window":"w"}])").find("run"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","run":{"cmd":""},"window":"w"}])").find("cmd"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","run":{"cmd":"a","args":[1]},"window":"w"}])").find("args"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","run":{"cmd":"a"}}])").find("window"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","run":{"cmd":"a"},"window":"w","fps":120}])").find("fps"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","run":{"cmd":"a"},"window":"w","bitrate":1}])").find("bitrate"), std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","run":{"cmd":"a"},"window":"w","buttons":[{"label":"x","key":"No pe"}]}])").find("No pe"),
              std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","run":{"cmd":"a"},"window":"w"},{"name":"a","run":{"cmd":"b"},"window":"v"}])")
                  .find("twice"),
              std::string::npos);
    EXPECT_NE(parse_err(R"([{"name":"a","build":{"cmd":"b","timeout_sec":0},"run":{"cmd":"a"},"window":"w"}])").find("timeout_sec"),
              std::string::npos);
}

TEST(PlayTarget, TheProfileKeepsPlayRaw) {
    std::string err;
    const auto  p = parse_workspace_profile(R"({"name":"g","play":[{"name":"g","run":{"cmd":"a"},"window":"w"}]})",
                                            "/x/g.studio.json", [](const std::string&) { return std::nullopt; }, err);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->play.is_array());
    EXPECT_EQ(p->play.a->size(), 1u);
    const auto none = parse_workspace_profile(R"({"name":"g"})", "/x/g.studio.json",
                                              [](const std::string&) { return std::nullopt; }, err);
    ASSERT_TRUE(none.has_value());
    EXPECT_TRUE(none->play.is_null());
}

// ---- messages -------------------------------------------------------

TEST(PlayMessage, ParsesAndClamps) {
    const auto offer = parse_play_message(R"({"type":"offer","target":"amicii","sdp":"v=0"})");
    ASSERT_TRUE(std::holds_alternative<OfferMessage>(offer));
    EXPECT_EQ(std::get<OfferMessage>(offer).target, "amicii");

    const auto key = parse_play_message(R"({"type":"key","key":"W","down":true})");
    ASSERT_TRUE(std::holds_alternative<KeyMessage>(key));
    EXPECT_EQ(std::get<KeyMessage>(key).name, "W");
    EXPECT_TRUE(std::get<KeyMessage>(key).down);
    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message(R"({"type":"key","key":"<script>","down":true})")));

    const auto ptr = parse_play_message(R"({"type":"pointer","x":1.5,"y":-2,"button":7,"phase":"down"})");
    ASSERT_TRUE(std::holds_alternative<PointerMessage>(ptr));
    EXPECT_EQ(std::get<PointerMessage>(ptr).x, 1.0);
    EXPECT_EQ(std::get<PointerMessage>(ptr).y, 0.0);
    EXPECT_EQ(std::get<PointerMessage>(ptr).button, 0);
    EXPECT_EQ(std::get<PointerMessage>(ptr).phase, PointerPhase::Down);
    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message(R"({"type":"pointer","x":0,"y":0,"phase":"drag"})")));

    const auto wheel = parse_play_message(R"({"type":"wheel","x":0.5,"y":0.5,"delta":-99})");
    ASSERT_TRUE(std::holds_alternative<WheelMessage>(wheel));
    EXPECT_EQ(std::get<WheelMessage>(wheel).notches, -10);
    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message(R"({"type":"wheel","delta":0})")));

    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message("not json")));
    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message(R"({"type":"exec","cmd":"x"})")));
    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message(R"({"type":"offer","target":"a"})")));
    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message(std::string(kMaxWsMessage + 1, ' '))));
    const std::string big_sdp = R"({"type":"offer","target":"a","sdp":")" + std::string(kMaxSdp + 1, 'x') + "\"}";
    EXPECT_TRUE(std::holds_alternative<BadMessage>(parse_play_message(big_sdp)));
}

TEST(PlayLines, SplitsAndCutsLongLines) {
    platform::LineSplitter   s(8);
    std::vector<std::string> got;
    const auto               emit = [&](std::string_view l) { got.emplace_back(l); };
    s.push("one\r\ntw", emit);
    s.push("o\n0123456789abc\nlast", emit);
    s.finish(emit);
    EXPECT_EQ(got, (std::vector<std::string>{"one", "two", "01234567", "last"}));
}

// ---- build / run -------------------------------------------------------------

namespace {

class FakeProcess final : public platform::ILiveProcess {
public:
    explicit FakeProcess(platform::LiveProcessHandlers h) : handlers(std::move(h)) {}
    void     terminate() override { terminated = true; }
    bool     running() const override { return alive.load(); }
    uint32_t pid() const override { return 42; }
    /// The test ends the process (never under the runner's lock).
    void finish(int code) {
        alive.store(false);
        if (handlers.on_exit) handlers.on_exit(code);
    }
    platform::LiveProcessHandlers handlers;
    std::atomic<bool>             alive{true};
    bool                          terminated = false;
};

struct Launcher {
    std::vector<platform::LiveProcessSpec> specs;
    std::vector<FakeProcess*>              made;
    bool                                   fail = false;
    TargetRunner::StartFn fn() {
        return [this](const platform::LiveProcessSpec& spec, platform::LiveProcessHandlers h, std::string& err)
                   -> std::unique_ptr<platform::ILiveProcess> {
            if (fail) {
                err = "program not found on PATH: " + spec.program;
                return nullptr;
            }
            specs.push_back(spec);
            auto p = std::make_unique<FakeProcess>(std::move(h));
            made.push_back(p.get());
            return p;
        };
    }
};

} // namespace

TEST(PlayRunner, BuildRunStopAndLogs) {
    Launcher                  launch;
    std::vector<std::string>  states;
    TargetRunner runner(launch.fn(), [&](const std::string& t, const TargetStatus& s) {
        states.push_back(t + ":" + state_name(s.state));
    });
    const auto  list = parse_ok(kTargets);
    std::string err;
    ASSERT_EQ(runner.build(list[0], err), StartResult::Started);
    EXPECT_EQ(launch.specs.at(0).program, "node");
    EXPECT_EQ(launch.specs.at(0).cwd, std::filesystem::path("/root"));
    EXPECT_EQ(runner.status("amicii").state, TargetState::Building);
    EXPECT_EQ(runner.run(list[0], err), StartResult::Busy);

    launch.made[0]->handlers.on_line(false, "compiling");
    launch.made[0]->finish(2);
    EXPECT_EQ(runner.status("amicii").state, TargetState::Failed);
    EXPECT_EQ(*runner.status("amicii").exit_code, 2);

    ASSERT_EQ(runner.run(list[0], err), StartResult::Started);
    EXPECT_EQ(launch.specs.at(1).args, list[0].run.args);
    EXPECT_TRUE(runner.stop("amicii"));
    EXPECT_TRUE(launch.made[1]->terminated);
    launch.made[1]->finish(-1);
    EXPECT_EQ(runner.status("amicii").state, TargetState::Idle);
    EXPECT_FALSE(runner.stop("amicii"));

    uint64_t   next  = 0;
    const auto lines = runner.logs("amicii", 0, next);
    ASSERT_TRUE(lines.size() >= 4u);
    EXPECT_EQ(lines[0].text, "> build: node");
    EXPECT_EQ(lines[1].text, "compiling");
    EXPECT_EQ(next, lines.back().n + 1);
    EXPECT_TRUE(runner.logs("amicii", next, next).empty());
    EXPECT_EQ(states, (std::vector<std::string>{"amicii:building", "amicii:failed", "amicii:running", "amicii:idle"}));

    EXPECT_EQ(runner.build(list[1], err), StartResult::NoBuild);
    launch.fail = true;
    EXPECT_EQ(runner.run(list[1], err), StartResult::Failed);
    EXPECT_NE(err.find("tool.exe"), std::string::npos);
    EXPECT_EQ(runner.status("tool").state, TargetState::Failed);
}

// ---- the plugin --------------------------------------------------------------

namespace {

class FakeRemote final : public IRemotePlay {
public:
    bool      streaming() const override { return true; }
    bool      input() const override { return true; }
    bool      turn() const override { return false; }
    JsonValue ice_servers(std::string*) override {
        JsonValue list = JsonValue::make_array();
        list.push(jobj({{"urls", jstr("stun:fake:3478")}}));
        return list;
    }
    bool open(const std::string& viewer, const PlayTarget& target, const std::string& offer, std::function<void()>,
              std::string& answer, std::string& err) override {
        if (offer == "reject") {
            err = "no_h264";
            return false;
        }
        opened.push_back(viewer + "@" + target.name);
        answer = "v=0 answer";
        return true;
    }
    void        close(const std::string& viewer) override { closed.push_back(viewer); }
    std::size_t viewers(const std::string&) const override { return opened.size() - closed.size(); }
    bool        known_key(const std::string& name) const override { return name == "W" || name == "Enter"; }
    bool key(const std::string& window, const std::string& name, bool down, std::string&) override {
        events.push_back(window + ":key:" + name + (down ? ":down" : ":up"));
        return true;
    }
    bool pointer(const std::string& window, double, double, int button, PointerPhase, std::string&) override {
        events.push_back(window + ":pointer:" + std::to_string(button));
        return true;
    }
    bool wheel(const std::string& window, double, double, int notches, std::string&) override {
        events.push_back(window + ":wheel:" + std::to_string(notches));
        return true;
    }
    std::vector<std::string> opened, closed, events;
};

class FakeConn final : public net::IWsConnection {
public:
    uint64_t id() const override { return 9; }
    void     send_text(const std::string& p) override { sent.push_back(p); }
    void     close(uint16_t code) override { closed = code; }
    bool     is_open() const override { return closed == 0; }
    std::vector<std::string> sent;
    uint16_t                 closed = 0;
};

net::HttpRequest request(const std::string& method, std::map<std::string, std::string> query = {}) {
    net::HttpRequest r;
    r.method = method;
    r.query  = std::move(query);
    return r;
}

struct Rig {
    Launcher                    launch;
    FakeRemote*                 remote = nullptr;
    std::unique_ptr<PlayPlugin> plugin;
    Rig() {
        PlayDeps deps;
        deps.targets = [](std::vector<std::string>&) { return parse_ok(kTargets); };
        auto r       = std::make_unique<FakeRemote>();
        remote       = r.get();
        deps.remote  = std::move(r);
        deps.start   = launch.fn();
        plugin       = std::make_unique<PlayPlugin>(std::move(deps), [](const std::string&, const std::string&) {});
    }
};

} // namespace

TEST(PlayPluginApi, TargetsActionsAndLogs) {
    Rig rig;
    auto& p = *rig.plugin;
    const auto list = p.handle(request("GET"), "/api/targets");
    EXPECT_EQ(list.status, 200);
    EXPECT_NE(list.body.find("\"amicii\""), std::string::npos);
    EXPECT_NE(list.body.find("\"can_build\":true"), std::string::npos);
    EXPECT_EQ(p.handle(request("POST"), "/api/targets").status, 405);

    EXPECT_EQ(p.handle(request("POST"), "/api/targets/amicii/run").status, 200);
    EXPECT_EQ(p.handle(request("POST"), "/api/targets/amicii/build").status, 409);
    EXPECT_EQ(p.handle(request("GET"), "/api/targets/amicii/run").status, 405);
    EXPECT_EQ(p.handle(request("POST"), "/api/targets/nope/run").status, 404);
    EXPECT_EQ(p.handle(request("POST"), "/api/targets/amicii/exec").status, 404);
    EXPECT_EQ(p.handle(request("POST"), "/api/targets/tool/build").status, 400);
    EXPECT_EQ(p.handle(request("POST"), "/api/targets/amicii/stop").status, 200);
    rig.launch.made[0]->finish(-1);
    EXPECT_EQ(p.handle(request("POST"), "/api/targets/amicii/stop").status, 409);

    const auto logs = p.handle(request("GET", {{"since", "0"}}), "/api/targets/amicii/logs");
    EXPECT_EQ(logs.status, 200);
    EXPECT_NE(logs.body.find("> run: node"), std::string::npos);
    const auto ice = p.handle(request("GET"), "/api/ice");
    EXPECT_NE(ice.body.find("stun:fake:3478"), std::string::npos);
    EXPECT_EQ(p.handle(request("GET"), "/api/health").status, 200);
    EXPECT_EQ(p.handle(request("GET"), "/api/nope").status, 404);
}

TEST(PlayPluginWs, InputOnlyAfterAnAnsweredOffer) {
    Rig  rig;
    auto conn = std::make_shared<FakeConn>();
    const auto h = rig.plugin->on_open(request("GET"), conn);

    h.on_text(R"({"type":"key","key":"W","down":true})");
    EXPECT_TRUE(rig.remote->events.empty());   // PLAY-INV-02

    h.on_text(R"({"type":"offer","target":"nope","sdp":"x"})");
    ASSERT_FALSE(conn->sent.empty());
    EXPECT_NE(conn->sent.back().find("unknown_target"), std::string::npos);
    h.on_text(R"({"type":"offer","target":"amicii","sdp":"reject"})");
    EXPECT_NE(conn->sent.back().find("no_h264"), std::string::npos);

    h.on_text(R"({"type":"offer","target":"amicii","sdp":"v=0 offer"})");
    EXPECT_NE(conn->sent.back().find("\"answer\""), std::string::npos);
    EXPECT_EQ(rig.remote->opened, (std::vector<std::string>{"ws-9@amicii"}));

    h.on_text(R"({"type":"key","key":"W","down":true})");
    h.on_text(R"({"type":"pointer","x":0.5,"y":0.5,"button":2,"phase":"down"})");
    h.on_text(R"({"type":"wheel","x":0.5,"y":0.5,"delta":3})");
    EXPECT_EQ(rig.remote->events, (std::vector<std::string>{"Amicii:key:W:down", "Amicii:pointer:2", "Amicii:wheel:3"}));
    h.on_text(R"({"type":"key","key":"F13","down":true})");   // a key the input does not know
    EXPECT_NE(conn->sent.back().find("unknown key"), std::string::npos);
    h.on_text(R"({"type":"bogus"})");
    EXPECT_NE(conn->sent.back().find("bad_message"), std::string::npos);

    // A state change reaches the socket.
    EXPECT_EQ(rig.plugin->handle(request("POST"), "/api/targets/amicii/run").status, 200);
    EXPECT_NE(conn->sent.back().find("\"running\""), std::string::npos);
    rig.launch.made[0]->finish(0);

    h.on_close();
    EXPECT_EQ(rig.remote->closed, (std::vector<std::string>{"ws-9"}));
}
