// The "sonus" plugin (spec/feature/tool/sonus.md) against a Sonus game running in this
// process: Sonus' C API with a null device and the live link on a loopback port
// (sonus_test_game.h). The test's thread is the game's loop; the plugin's ticker
// thread polls the tool session meanwhile, as in the Studio.
#include "plugins/sonus/recording_path.h"
#include "plugins/sonus/sonus_plugin.h"
#include "plugins/sonus/sonus_shown_json.h"
#include "sonus_test_game.h"

#include "plugins/common/json_util.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <thread>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::sonus;
using sonus_test::Game;
using sonus_test::TempDir;

namespace {

/// The game's loop: one frame per millisecond until pred() (or 60 s).
template <class Pred>
bool run_until(Game& game, Pred pred, double seconds = 60.0) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    for (;;) {
        game.step();
        if (pred()) return true;
        if (std::chrono::steady_clock::now() > end) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

const BusShown* bus_named(const SonusShown& s, const char* name) {
    for (const BusShown& b : s.buses) {
        if (b.name == name) return &b;
    }
    return nullptr;
}

float studio_master_peak(const SonusPlugin& p) {
    const SonusShown s = p.shown();
    const BusShown*  m = bus_named(s, "Master");
    return m && m->meter.present && !m->meter.stale ? m->meter.peak : -1.0f;
}

net::HttpRequest request(const std::string& method, const std::string& sub, const std::string& body,
                         const std::string& host = "127.0.0.1:5170") {
    net::HttpRequest r;
    r.method          = method;
    r.path            = "/sonus" + sub;
    r.target          = r.path;
    r.headers["host"] = host;
    if (!body.empty()) r.headers["content-type"] = "application/json";
    r.body = body;
    return r;
}

/// The response body as JSON (null when it is not JSON; the field checks then fail).
JsonValue body_of(const net::HttpResponse& res) {
    JsonValue v;
    if (!ergo::common::jsonm::parse(res.body, v)) return JsonValue::make_null();
    return v;
}

struct Log {
    std::mutex               mtx;
    std::vector<std::string> lines;
    host::LogFn fn() {
        return [this](const std::string& level, const std::string& msg) {
            std::lock_guard<std::mutex> lk(mtx);
            lines.push_back(level + " " + msg);
        };
    }
    bool mentions(const std::string& part) {
        std::lock_guard<std::mutex> lk(mtx);
        for (const auto& l : lines) {
            if (l.find(part) != std::string::npos) return true;
        }
        return false;
    }
};

/// The project, its bank, the game on a free port, and the plugin's settings for it.
struct Rig {
    TempDir                    dir{"rig"};
    std::string                error;
    std::unique_ptr<Game>      game;
    SonusConfig                config;
    float                      level = 0.0f;   // the game's steady master peak with Tone + Pad

    Rig() {
        sonus_test::write_mix_project(dir.path("project"));
        const std::string bank = sonus_test::build_project(dir.path("project"), dir.path("game-build"), error);
        if (bank.empty()) return;
        for (int i = 0; i < 32 && !game; ++i) {
            config.port = sonus_test::candidate_port(i);
            game        = Game::start(bank, config.port, error);
        }
        if (!game) return;
        config.host          = "127.0.0.1";
        config.project_dir   = dir.path("project");
        config.build_dir     = dir.path("studio-build");   // the same project: same build hash, no mismatch
        config.recording_dir = dir.path("recordings");
        config.tick_ms       = 10;
        game->play("Tone");
        game->play("Pad");
        // The steady output: the master meter unchanged over 32 frames in a row (the first
        // meters still hold the start of the sounds).
        float last = -1.0f;
        for (int same = 0, i = 0; same < 32 && i < 20000; ++i) {
            game->step();
            const float now = game->master_peak();
            same = std::fabs(now - last) < 1e-6f ? same + 1 : 0;
            last = now;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        level = last;
    }
};

} // namespace

TEST(StudioSonusConfig, RecordingFileNamesAreUtcAndNumbered) {
    EXPECT_EQ(recording_file_name(0, 1), "sonus-19700101-000000-1.snrc");
    EXPECT_EQ(recording_file_name(1795000000, 12), "sonus-20261118-110640-12.snrc");
}

TEST(StudioSonusPlugin, OfflineWithoutAProject) {
    Log log;
    SonusConfig c;
    c.port = 1;   // never dialled: nothing connects in this test
    SonusPlugin plugin(c, log.fn());
    EXPECT_EQ(plugin.info().id, "sonus");
    EXPECT_TRUE(plugin.http() != nullptr);
    EXPECT_TRUE(plugin.health() != nullptr);
    EXPECT_TRUE(plugin.socket() == nullptr);
    const SonusShown s = plugin.shown();
    EXPECT_TRUE(s.state == LinkState::Offline);
    EXPECT_TRUE(s.buses.empty());
    EXPECT_FALSE(s.project_loaded);
    JsonValue h = plugin.snapshot();
    EXPECT_TRUE(field(h, "ok")->as_bool());
    EXPECT_EQ(field(h, "state")->as_string(), std::string("offline"));
    EXPECT_EQ(field(h, "target")->as_string(), std::string("127.0.0.1:1"));
    // Edits without a project have nothing to act on; they say so.
    EXPECT_TRUE(plugin.set_bus_mute("anything", true) == Edit::NotFound);
    EXPECT_TRUE(plugin.revert("bus.1.vol") == Edit::NotFound);
    EXPECT_TRUE(plugin.undo() == Edit::Unchanged);
    EXPECT_TRUE(plugin.revert_all() == Edit::Unchanged);
    EXPECT_FALSE(plugin.start_recording());   // no game
    EXPECT_FALSE(plugin.stop_recording());
    std::string why;
    EXPECT_TRUE(plugin.save_recording(why).empty());
    EXPECT_NE(why.find("ERGO_SONUS_RECORDING_DIR"), std::string::npos);

    // Routes: saving off is 503, the rest of the contract as the lesson plugin's.
    host::IPluginHttp& routes = *plugin.http();
    EXPECT_EQ(routes.handle(request("POST", "/api/recording/save", "{}"), "/api/recording/save").status, 503);
    EXPECT_EQ(routes.handle(request("GET", "/api/nothing", ""), "/api/nothing").status, 404);
    EXPECT_EQ(routes.handle(request("POST", "/api/state", "{}"), "/api/state").status, 405);
    EXPECT_EQ(routes.handle(request("GET", "/api/connect", ""), "/api/connect").status, 405);
    EXPECT_EQ(routes.handle(request("POST", "/api/disconnect", "{}", "evil.example:80"), "/api/disconnect").status, 403);
    net::HttpRequest cross = request("POST", "/api/mixer/undo", "{}");
    cross.headers["origin"] = "https://evil.example";
    EXPECT_EQ(routes.handle(cross, "/api/mixer/undo").status, 403);
    net::HttpRequest form = request("POST", "/api/mixer/undo", "a=b");
    form.headers["content-type"] = "application/x-www-form-urlencoded";
    EXPECT_EQ(routes.handle(form, "/api/mixer/undo").status, 403);
    EXPECT_EQ(routes.handle(request("POST", "/api/mixer/undo", "[1]"), "/api/mixer/undo").status, 400);
    EXPECT_EQ(routes.handle(request("POST", "/api/mixer/bus", R"({"bus":"x"})"), "/api/mixer/bus").status, 400);
    EXPECT_EQ(routes.handle(request("POST", "/api/mixer/bus", R"({"bus":"x","mute":true,"solo":true})"), "/api/mixer/bus").status,
              400);
    EXPECT_EQ(routes.handle(request("POST", "/api/mixer/bus", R"({"bus":"x","mute":1})"), "/api/mixer/bus").status, 400);
    EXPECT_EQ(routes.handle(request("POST", "/api/mixer/bus", R"({"bus":"x","mute":true})"), "/api/mixer/bus").status, 404);
    EXPECT_EQ(routes.handle(request("POST", "/api/overrides/revert", "{}"), "/api/overrides/revert").status, 400);
    EXPECT_EQ(routes.handle(request("POST", "/api/recording/start", "{}"), "/api/recording/start").status, 409);
    EXPECT_TRUE(log.mentions("rejected POST /api/disconnect"));
}

TEST(StudioSonusPlugin, AProjectWithErrorsIsReported) {
    TempDir dir("broken");
    sonus_test::write_mix_project(dir.path("project"));
    {
        std::ofstream f(std::filesystem::path(dir.path("project")) / "buses" / "Bad.json");
        f << "{ not json";
    }
    Log log;
    SonusConfig c;
    c.project_dir = dir.path("project");
    SonusPlugin plugin(c, log.fn());
    EXPECT_FALSE(plugin.shown().project_errors.empty());
    EXPECT_FALSE(field(plugin.snapshot(), "ok")->as_bool(true));
    EXPECT_TRUE(log.mentions("has errors"));
}

TEST(StudioSonusPlugin, MixerProfilerOverridesAndRecordingAgainstALiveGame) {
    Rig rig;
    if (!rig.game) std::printf("the game did not start: %s\n", rig.error.c_str());
    ASSERT_TRUE(rig.game != nullptr);
    ASSERT_TRUE(rig.level > 0.1f);
    Game& game        = *rig.game;
    const float level = rig.level;
    const float tol   = level * 2e-3f;

    Log         log;
    SonusPlugin plugin(rig.config, log.fn());
    host::IPluginHttp& routes = *plugin.http();
    auto call = [&](const std::string& method, const std::string& sub, const std::string& body) {
        return routes.handle(request(method, sub, body), sub);
    };
    {
        const SonusShown s = plugin.shown();
        ASSERT_TRUE(s.project_loaded);
        EXPECT_EQ(s.project_name, "Mix");
        ASSERT_EQ(s.buses.size(), 3u);
        EXPECT_TRUE(s.buses[0].master);
        EXPECT_EQ(s.buses[0].name, "Master");
    }
    const std::string sfx   = bus_named(plugin.shown(), "Sfx")->key;
    const std::string music = bus_named(plugin.shown(), "Music")->key;

    // ---- connect (through the route) and the profiler --------------------------------
    EXPECT_EQ(call("POST", "/api/connect", "{}").status, 200);
    EXPECT_EQ(call("POST", "/api/connect", "{}").status, 409);   // already open
    ASSERT_TRUE(run_until(game, [&] { return plugin.shown().state == LinkState::Online; }));
    EXPECT_FALSE(plugin.shown().mismatch);
    EXPECT_EQ(plugin.shown().sample_rate, 48000u);
    const bool meter_ok = run_until(game, [&] { return near(studio_master_peak(plugin), level, tol); });
    if (!meter_ok) {
        std::printf("level %f studio %f game now %f\n", level, studio_master_peak(plugin), game.master_peak());
        for (const MeterRow& m : plugin.shown().meters) std::printf("meter %08x %s %f stale %d\n", m.bus_id, m.name.c_str(), m.meter.peak, m.meter.stale);
    }
    ASSERT_TRUE(meter_ok);
    ASSERT_TRUE(run_until(game, [&] {
        const SonusShown s = plugin.shown();
        return s.real_voices >= 2u && s.timing.samples > 0u && s.has_memory && !s.meters.empty();
    }));
    {
        const SonusShown s = plugin.shown();
        bool tone_on_sfx = false;
        for (const VoiceShown& v : s.voices) tone_on_sfx = tone_on_sfx || v.bus == "Sfx";
        EXPECT_TRUE(tone_on_sfx);
        EXPECT_GT(s.timing.budget_us, 0.0f);
        EXPECT_FALSE(s.memory.empty());
        bool named = false;
        for (const MeterRow& m : s.meters) named = named || m.name == "Sfx" || m.name == "output";
        EXPECT_TRUE(named);
    }
    game.play("Blip");   // an event while connected: it shows in the log
    ASSERT_TRUE(run_until(game, [&] { return !plugin.shown().log.empty(); }));

    // ---- mute: the game's output changes, the Studio's meter follows ----------------
    EXPECT_EQ(call("POST", "/api/mixer/bus", R"({"bus":")" + sfx + R"(","mute":true})").status, 200);
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), level * 0.5f, tol); }));
    // ... and so does what the device plays.
    EXPECT_TRUE(run_until(game, [&] { return near(game.device_peak(), level * 0.5f, tol); }));
    ASSERT_TRUE(run_until(game, [&] { return near(studio_master_peak(plugin), level * 0.5f, tol); }));
    {
        const SonusShown s = plugin.shown();
        EXPECT_TRUE(bus_named(s, "Sfx")->muted);
        EXPECT_EQ(bus_named(s, "Sfx")->effective_db, kFloorDb);
        ASSERT_EQ(s.overrides.size(), 1u);
        EXPECT_TRUE(s.overrides[0].muted);
        EXPECT_EQ(s.undo_count, 1u);
    }

    // ---- revert all: everything back to the project ----------------------------------
    EXPECT_EQ(call("POST", "/api/overrides/revert-all", "{}").status, 200);
    EXPECT_TRUE(plugin.shown().overrides.empty());
    EXPECT_FALSE(bus_named(plugin.shown(), "Sfx")->muted);
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), level, tol); }));

    // ---- fader, undo / redo ----------------------------------------------------------
    const float music_6db = level * 0.5f + level * 0.5f * static_cast<float>(std::pow(10.0, -6.0 / 20.0));
    EXPECT_TRUE(plugin.set_bus_fader(music, -6.0f) == Edit::Changed);
    EXPECT_TRUE(plugin.set_bus_fader(music, -6.0f) == Edit::Unchanged);
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), music_6db, tol); }));
    EXPECT_EQ(call("POST", "/api/mixer/undo", "{}").status, 200);
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), level, tol); }));
    EXPECT_TRUE(plugin.redo() == Edit::Changed);
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), music_6db, tol); }));
    EXPECT_TRUE(plugin.nudge_bus_fader(music, 6.0f) == Edit::Changed);   // -6 + 6 = 0 dB
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), level, tol); }));
    EXPECT_NEAR(bus_named(plugin.shown(), "Music")->fader_db, 0.0f, 1e-4);

    // ---- solo, then revert that one override by its key ------------------------------
    EXPECT_EQ(call("POST", "/api/mixer/bus", R"({"bus":")" + music + R"(","solo":true})").status, 200);
    EXPECT_TRUE(bus_named(plugin.shown(), "Sfx")->solo_muted);
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), level * 0.5f, tol); }));
    std::string sfx_override;
    for (const OverrideShown& o : plugin.shown().overrides) {
        if (o.muted) sfx_override = o.key;
    }
    ASSERT_FALSE(sfx_override.empty());
    EXPECT_EQ(call("POST", "/api/overrides/revert", R"({"key":")" + sfx_override + R"("})").status, 200);
    EXPECT_EQ(call("POST", "/api/overrides/revert", R"({"key":"no.such.key"})").status, 404);
    ASSERT_TRUE(run_until(game, [&] { return near(game.master_peak(), level, tol); }));
    EXPECT_TRUE(plugin.revert_all() == Edit::Unchanged || plugin.shown().overrides.empty());

    // ---- recording: start, stop, save --------------------------------------------------
    EXPECT_EQ(call("POST", "/api/recording/save", "{}").status, 409);   // nothing recorded yet
    EXPECT_EQ(call("POST", "/api/recording/start", "{}").status, 200);
    ASSERT_TRUE(run_until(game, [&] { return plugin.shown().recording.state == "recording"; }));
    ASSERT_TRUE(run_until(game, [&] { return plugin.shown().recording.bytes > 0u; }));
    EXPECT_EQ(call("POST", "/api/recording/stop", "{}").status, 200);
    ASSERT_TRUE(run_until(game, [&] { return plugin.shown().recording.can_save; }));
    const net::HttpResponse saved = call("POST", "/api/recording/save", "{}");
    ASSERT_EQ(saved.status, 200);
    const std::string path = field(body_of(saved), "path")->as_string();
    EXPECT_EQ(path, plugin.last_saved());
    EXPECT_EQ(std::filesystem::path(path).parent_path(), std::filesystem::path(rig.config.recording_dir));
    std::error_code ec;
    EXPECT_GT(std::filesystem::file_size(path, ec), 64u);   // the header plus records
    EXPECT_TRUE(log.mentions("recording saved"));

    // ---- state route and health -----------------------------------------------------
    const net::HttpResponse st = call("GET", "/api/state", "");
    ASSERT_EQ(st.status, 200);
    const JsonValue j = body_of(st);
    EXPECT_EQ(field(j, "state")->as_string(), std::string("online"));
    EXPECT_EQ(field(*field(j, "mixer"), "buses")->a->size(), 3u);
    EXPECT_TRUE(field(*field(j, "profiler"), "timing") != nullptr);
    EXPECT_EQ(field(*field(j, "recording"), "state")->as_string(), std::string("ready"));
    EXPECT_EQ(field(plugin.snapshot(), "state")->as_string(), std::string("online"));
    EXPECT_TRUE(log.mentions("[sonus] link online"));

    // ---- disconnect ------------------------------------------------------------------
    EXPECT_EQ(call("POST", "/api/disconnect", "{}").status, 200);
    EXPECT_TRUE(plugin.shown().state == LinkState::Offline);
    for (int i = 0; i < 16; ++i) game.step();
    EXPECT_TRUE(near(game.master_peak(), level, tol));   // the game keeps the project's mix
}
