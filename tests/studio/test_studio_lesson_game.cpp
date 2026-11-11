// The presenter's reconciliation against a real game link: ergo_bind (the
// engine side) connected over loopback to the Studio's own server and the
// variable plugin, with the lesson's DemoApplier on the bridge. Proves that a
// set requested by the lesson comes back as the game's published value (so
// "applied" means the game really holds it), that a restarted game is put
// back, and that a read_only variable shows up as "not following".
#include "lesson_variables.h"
#include "plugins/lesson/demo_apply.h"

#include "ergo/bind/bind.h"
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/net/server.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <thread>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::lesson;

namespace {

void quiet(const std::string&, const std::string&) {}

/// Runs the game's frame (apply_pending_writes) until `done` or 10 s.
bool pump_until(const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        ergo::bind::Engine::instance().apply_pending_writes();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

std::optional<bool> published(const app::GameVariableBridge& bridge, const char* name) {
    const auto v = bridge.find(name);
    if (!v || !v->value.is_bool()) return std::nullopt;
    return v->value.b;
}

} // namespace

TEST(StudioLessonGame, DemoSwitchesFollowARealBindEngine) {
    auto bridge = std::make_shared<app::GameVariableBridge>();
    host::LoadResult loaded;
    loaded.plugins.push_back({app::make_bridged_variable_plugin(bridge, quiet), "built-in", true});
    host::PluginHost plugins(std::move(loaded), quiet);
    net::Server      server({"127.0.0.1", 0}, plugins, plugins, quiet);
    std::string      err;
    ASSERT_TRUE(server.start(err));

    bool hit_stop = false;
    bool locked   = false;
    ergo::bind::VarMeta read_only;
    read_only.read_only = true;
    auto& engine = ergo::bind::Engine::instance();
    engine.set_app_name("lesson-game-test");
    engine.bind("feel.hit_stop", &hit_stop);
    engine.bind("feel.locked", &locked, read_only);
    engine.connect("127.0.0.1", server.port(), "/variable/ws");
    ASSERT_TRUE(pump_until([&] { return published(*bridge, "feel.hit_stop") && published(*bridge, "feel.locked"); }));

    DemoApplier demo;
    demo.observe({{"hit_stop", true}, {"locked", true}}, {{"hit_stop", "feel.hit_stop"}, {"locked", "feel.locked"}});
    demo.apply(*bridge);
    ASSERT_EQ(demo.entries().size(), 2u);
    EXPECT_TRUE(demo.entries()[0].status == DemoStatus::Requested);   // hit_stop
    EXPECT_TRUE(demo.entries()[1].status == DemoStatus::Requested);   // locked

    // The engine applies the set on its frame and publishes what it now holds.
    ASSERT_TRUE(pump_until([&] { return published(*bridge, "feel.hit_stop") == std::optional<bool>(true); }));
    EXPECT_TRUE(hit_stop);
    EXPECT_FALSE(locked);   // read_only: the engine ignores the set
    demo.apply(*bridge);
    EXPECT_TRUE(demo.entries()[0].status == DemoStatus::Applied);
    EXPECT_TRUE(demo.entries()[1].status == DemoStatus::NotFollowing);

    // The game restarts mid-demo: it binds again at its default.
    engine.disconnect();
    ASSERT_TRUE(pump_until([&] { return !published(*bridge, "feel.hit_stop").has_value(); }));
    demo.apply(*bridge);
    EXPECT_TRUE(demo.entries()[0].status == DemoStatus::WaitingForGame);
    hit_stop = false;
    engine.connect("127.0.0.1", server.port(), "/variable/ws");
    ASSERT_TRUE(pump_until([&] { return published(*bridge, "feel.hit_stop") == std::optional<bool>(false); }));
    demo.apply(*bridge);
    EXPECT_TRUE(demo.entries()[0].status == DemoStatus::Requested);
    ASSERT_TRUE(pump_until([&] { return published(*bridge, "feel.hit_stop") == std::optional<bool>(true); }));
    demo.apply(*bridge);
    EXPECT_TRUE(demo.entries()[0].status == DemoStatus::Applied);
    EXPECT_TRUE(hit_stop);

    engine.disconnect();
    server.stop();
}
