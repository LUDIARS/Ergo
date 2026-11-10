#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/ui/studio_app.h"
#include "ergo/studio/ui/view_builder.h"

#include <gtest/gtest.h>

#include <atomic>

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
