#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/ui/studio_app.h"
#include "ergo/studio/ui/view_builder.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>

using namespace ergo::studio;
using namespace ergo::studio::ui;

namespace {

/// A plugin with its own view: a counter button.
class CounterPlugin final : public host::IPlugin, public host::IPluginView {
public:
    const host::PluginInfo& info() const override { return info_; }
    host::IPluginView*      view() override { return this; }
    uint64_t view_revision() const override { return clicks; }
    void     build(ViewBuilder& b) override {
        b.title("Counter");
        auto row = b.row();
        row.label("clicks " + std::to_string(clicks));
        row.button("add", [this] { ++clicks; });
        b.label("after the row");   // a sibling added after the row builder was used
        row.label("late");          // ...and the row is still usable
    }
    uint64_t clicks = 0;

private:
    host::PluginInfo info_{"counter", "Counter", "", ""};
};

class PlainPlugin final : public host::IPlugin {
public:
    const host::PluginInfo& info() const override { return info_; }

private:
    host::PluginInfo info_{"plain", "Plain", "no view", ""};
};

const ergo::ui_kit::DrawCmd* find_text(const std::vector<ergo::ui_kit::DrawCmd>& list, const std::string& text) {
    for (const auto& c : list) {
        if (c.kind == ergo::ui_kit::DrawKind::Text && c.text == text) return &c;
    }
    return nullptr;
}

void click_at(StudioApp& app, float x, float y) {
    app.frame({x, y, false}, 0.016f);
    app.frame({x, y, true}, 0.016f);
    app.frame({x, y, false}, 0.016f);
    app.frame({x, y, false}, 0.016f);   // rebuild after the click
}

struct Fixture {
    Fixture() {
        auto loaded = host::instantiate_plugins(
            {[](const host::LogFn&) { return std::make_unique<CounterPlugin>(); },
             [](const host::LogFn&) { return std::make_unique<PlainPlugin>(); }},
            {}, [](const std::string&, const std::string&) {});
        host = std::make_unique<host::PluginHost>(std::move(loaded), [](const std::string&, const std::string&) {});
    }
    std::unique_ptr<host::PluginHost> host;
    FixedAdvanceMeasure               measure;
};

} // namespace

TEST(StudioUi, OverviewListsPluginsAndTabsSwitch) {
    Fixture f;
    StudioApp app(*f.host, f.measure);
    app.set_viewport(1000, 700);
    app.frame({}, 0.016f);
    EXPECT_EQ(app.tab_count(), 3u);
    ASSERT_NE(find_text(app.draw_list(), "Ergo Studio"), nullptr);
    ASSERT_NE(find_text(app.draw_list(), "Plain  (plain)"), nullptr);

    const auto* tab = find_text(app.draw_list(), "Counter");
    ASSERT_NE(tab, nullptr);
    const auto r = tab->rect;
    click_at(app, r.x + r.w / 2, r.y + r.h / 2);
    EXPECT_EQ(app.active_tab(), 1u);
    ASSERT_NE(find_text(app.draw_list(), "clicks 0"), nullptr);
    EXPECT_NE(find_text(app.draw_list(), "late"), nullptr);
    EXPECT_NE(find_text(app.draw_list(), "after the row"), nullptr);
}

TEST(StudioUi, ViewButtonsRunAndRevisionRebuilds) {
    Fixture f;
    StudioApp app(*f.host, f.measure);
    app.set_viewport(1000, 700);
    app.select_tab(1);
    app.frame({}, 0.016f);
    const auto* add = find_text(app.draw_list(), "add");
    ASSERT_NE(add, nullptr);
    const auto r = add->rect;
    click_at(app, r.x + r.w / 2, r.y + r.h / 2);
    EXPECT_NE(find_text(app.draw_list(), "clicks 1"), nullptr);
}

TEST(StudioUi, PluginWithoutViewGetsTheGenericTab) {
    Fixture f;
    StudioApp app(*f.host, f.measure);
    app.select_tab(2);
    app.frame({}, 0.016f);
    EXPECT_NE(find_text(app.draw_list(), "no view"), nullptr);
    EXPECT_NE(find_text(app.draw_list(), "ok"), nullptr);
}

TEST(StudioUi, ScrollMovesContentUp) {
    Fixture f;
    StudioApp app(*f.host, f.measure);
    app.frame({}, 0.016f);
    const float y0 = find_text(app.draw_list(), "Ergo Studio")->rect.y;
    app.frame({0, 0, false, -2}, 0.016f);
    app.frame({}, 0.016f);
    const float y1 = find_text(app.draw_list(), "Ergo Studio")->rect.y;
    EXPECT_LT(y1, y0);
    EXPECT_EQ(FixedAdvanceMeasure(0.5f).width("ab", 10), 10.0f);
    EXPECT_EQ(FixedAdvanceMeasure(0.5f).width("\xE3\x81\x82", 10), 5.0f);   // one code point
}

TEST(StudioUi, ClickSurvivesARebuildBetweenPressAndRelease) {
    Fixture f;
    StudioApp app(*f.host, f.measure);
    app.set_viewport(1000, 700);
    app.set_health_interval(std::chrono::milliseconds(1));   // overview refreshes every frame
    app.frame({}, 0.016f);
    const auto* tab = find_text(app.draw_list(), "Plain");
    ASSERT_NE(tab, nullptr);
    const float x = tab->rect.x + tab->rect.w / 2, y = tab->rect.y + tab->rect.h / 2;
    app.frame({x, y, false}, 0.016f);
    app.frame({x, y, true}, 0.016f);
    app.frame({x, y, false}, 0.5f);   // the release frame is also a refresh frame
    app.frame({x, y, false}, 0.016f);
    EXPECT_EQ(app.active_tab(), 2u);
}

// ---- the launcher (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A / B) ----

namespace {

namespace fs = std::filesystem;

/// A launcher over one profile file in a fresh folder (any existing file is a profile).
struct LauncherFixture {
    LauncherFixture() {
        static std::atomic<int> n{0};
        dir = fs::temp_directory_path() / ("ergo-ui-launcher-" + std::to_string(++n));
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir);
        std::ofstream(dir / "game.json") << "{}";
        project::LauncherDeps deps;
        deps.probe = [](const fs::path& source, std::string&) -> std::optional<project::ProjectRef> {
            return project::ProjectRef{source.u8string(), "game", "D:/game", "cpp-ludiars", "game"};
        };
        deps.discover = [d = dir] {
            return std::vector<project::ProjectRef>{{(d / "game.json").u8string(), "game", "D:/game", "cpp-ludiars", "game"}};
        };
        deps.now = [] { return std::string("2026-11-23T10:00:00Z"); };
        launcher = std::make_unique<project::Launcher>(project::ProjectCatalog(dir / "recent_projects.json"), open,
                                                       std::move(deps));
    }
    fs::path                           dir;
    project::OpenProject               open;
    std::unique_ptr<project::Launcher> launcher;
};

/// A dev server that records start / stop and opens the project the way the real one does.
class FakeDevServer final : public project::IDevServerControl {
public:
    explicit FakeDevServer(project::Launcher& l) : launcher_(l) {}
    bool start(const std::string& source, std::string& err) override {
        if (!launcher_.open(source, err)) return false;
        running_ = true;
        ++revision_;
        return true;
    }
    void stop() override {
        running_ = false;
        launcher_.close();
        ++revision_;
    }
    project::DevServerStatus status() const override {
        project::DevServerStatus s;
        s.running = running_;
        if (running_) {
            if (const auto c = launcher_.project().current()) s.source = c->source;
            s.local_url = "http://127.0.0.1:5170/";
        }
        return s;
    }
    uint64_t revision() const override { return revision_; }

private:
    project::Launcher& launcher_;
    bool               running_  = false;
    uint64_t           revision_ = 0;
};

void click_text(StudioApp& app, const std::string& text) {
    const auto* t = find_text(app.draw_list(), text);
    ASSERT_NE(t, nullptr);
    click_at(app, t->rect.x + t->rect.w / 2, t->rect.y + t->rect.h / 2);
}

} // namespace

TEST(StudioUiLauncher, StartsOnTheLauncherAloneWithoutTabs) {
    Fixture f;
    LauncherFixture l;
    FakeDevServer dev(*l.launcher);
    StudioApp app(*f.host, f.measure, Theme{}, {}, l.launcher.get(), &dev);
    app.set_viewport(1000, 700);
    app.frame({}, 0.016f);
    EXPECT_TRUE(app.showing_launcher());
    EXPECT_NE(find_text(app.draw_list(), "game"), nullptr);
    EXPECT_NE(find_text(app.draw_list(), "リモート開発サーバ起動"), nullptr);
    EXPECT_EQ(find_text(app.draw_list(), "開く"), nullptr);   // the window serves projects, it does not open tabs
    EXPECT_EQ(find_text(app.draw_list(), "Counter"), nullptr);   // no tab list
    EXPECT_EQ(find_text(app.draw_list(), "Plain"), nullptr);
}

TEST(StudioUiLauncher, StartingTheServerKeepsTheLauncherAndStopCloses) {
    Fixture f;
    LauncherFixture l;
    FakeDevServer dev(*l.launcher);
    StudioApp app(*f.host, f.measure, Theme{}, {}, l.launcher.get(), &dev);
    app.set_viewport(1000, 700);
    app.frame({}, 0.016f);
    click_text(app, "リモート開発サーバ起動");
    app.frame({}, 0.016f);
    ASSERT_TRUE(l.open.current().has_value());
    EXPECT_TRUE(app.showing_launcher());   // the tabs are on the web side
    EXPECT_EQ(find_text(app.draw_list(), "Counter"), nullptr);
    EXPECT_NE(find_text(app.draw_list(), "手元: http://127.0.0.1:5170/"), nullptr);
    EXPECT_NE(find_text(app.draw_list(), "停止"), nullptr);

    click_text(app, "停止");
    app.frame({}, 0.016f);
    EXPECT_FALSE(l.open.current().has_value());
    EXPECT_NE(find_text(app.draw_list(), "リモート開発サーバ起動"), nullptr);
}

TEST(StudioUiLauncher, OpeningFromElsewhereKeepsTheWindowOnTheLauncher) {
    Fixture f;
    LauncherFixture l;
    FakeDevServer dev(*l.launcher);
    StudioApp app(*f.host, f.measure, Theme{}, {}, l.launcher.get(), &dev);
    app.frame({}, 0.016f);
    std::string err;
    ASSERT_TRUE(l.launcher->open((l.dir / "game.json").u8string(), err));   // e.g. POST /api/launcher/open
    app.frame({}, 0.016f);
    EXPECT_TRUE(app.showing_launcher());
    EXPECT_EQ(find_text(app.draw_list(), "Counter"), nullptr);
}
