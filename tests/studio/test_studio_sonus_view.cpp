// The サウンド tab (tools/studio/views/sonus_view): its pages, texts and buttons,
// built against a plugin without a game and against a live one.
#include "plugins/sonus/sonus_plugin.h"
#include "sonus_test_game.h"
#include "views/sonus_view.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <thread>

using namespace ergo::studio;
using namespace ergo::studio::plugins::sonus;
using views::SonusView;

namespace {

struct Built {
    std::vector<std::string>      texts;
    std::vector<ui::ClickBinding> clicks;
    int                           meters = 0;

    bool mentions(const std::string& part) const {
        for (const auto& t : texts) {
            if (t.find(part) != std::string::npos) return true;
        }
        return false;
    }
    /// Clicks the button whose text is `label` (buttons hold one Text child).
    bool click(const ergo::ui_kit::Node& root, const std::string& label) const {
        for (const auto& c : clicks) {
            if (button_text(root, c.path) == label) {
                c.fn();
                return true;
            }
        }
        return false;
    }
    static std::string button_text(const ergo::ui_kit::Node& root, const std::string& path) {
        const ergo::ui_kit::Node* n = &root;
        std::size_t               at = path.find('/');
        while (at != std::string::npos && n) {
            const std::size_t next = path.find('/', at + 1);
            const std::string name = path.substr(at + 1, next == std::string::npos ? std::string::npos : next - at - 1);
            const ergo::ui_kit::Node* found = nullptr;
            for (const auto& c : n->children) {
                if (c.name == name) found = &c;
            }
            n  = found;
            at = next;
        }
        return n && !n->children.empty() ? n->children[0].text : std::string();
    }
};

void collect(const ergo::ui_kit::Node& n, Built& out) {
    if (n.kind == ergo::ui_kit::NodeKind::Text) out.texts.push_back(n.text);
    if (n.name == "meter") ++out.meters;
    for (const auto& c : n.children) collect(c, out);
}

struct Tab {
    ergo::ui_kit::Node root;
    Built              built;
};

Tab build(SonusView& view) {
    Tab                     t;
    ui::FixedAdvanceMeasure measure;
    ui::Theme               theme;
    ui::ViewBuilder         b(t.root, "Root", measure, theme, t.built.clicks);
    view.build(b);
    collect(t.root, t.built);
    return t;
}

} // namespace

TEST(StudioSonusView, PagesWithoutAGame) {
    SonusConfig c;
    c.port = 1;
    SonusPlugin plugin(c, nullptr);
    SonusView   view(plugin);
    Tab t = build(view);
    EXPECT_TRUE(t.built.mentions("未接続"));
    EXPECT_TRUE(t.built.mentions("127.0.0.1:1"));
    EXPECT_TRUE(t.built.mentions("プロジェクトなし"));
    EXPECT_TRUE(t.built.mentions("バスがありません"));

    const uint64_t before = view.view_revision();
    EXPECT_TRUE(t.built.click(t.root, "プロファイラ"));
    EXPECT_TRUE(view.page() == SonusView::Page::Profiler);
    EXPECT_GT(view.view_revision(), before);
    t = build(view);
    EXPECT_TRUE(t.built.mentions("処理時間"));
    EXPECT_TRUE(t.built.mentions("ボイス"));
    EXPECT_TRUE(t.built.mentions("メモリ"));
    EXPECT_TRUE(t.built.mentions("ログ"));

    EXPECT_TRUE(t.built.click(t.root, "上書き (0)"));
    t = build(view);
    EXPECT_TRUE(t.built.mentions("上書きはありません"));

    EXPECT_TRUE(t.built.click(t.root, "録画"));
    t = build(view);
    EXPECT_TRUE(t.built.mentions("録画していません"));
    EXPECT_TRUE(t.built.mentions("ERGO_SONUS_RECORDING_DIR"));
    EXPECT_TRUE(t.built.click(t.root, "開始"));   // no game: the tab says why
    t = build(view);
    EXPECT_TRUE(t.built.mentions("録画を始められません"));
}

TEST(StudioSonusView, MixerStripsAgainstALiveGame) {
    sonus_test::TempDir dir("view");
    sonus_test::write_mix_project(dir.path("project"));
    std::string       error;
    const std::string bank = sonus_test::build_project(dir.path("project"), dir.path("game-build"), error);
    ASSERT_FALSE(bank.empty());
    std::unique_ptr<sonus_test::Game> game;
    SonusConfig                       c;
    for (int i = 0; i < 32 && !game; ++i) {
        c.port = sonus_test::candidate_port(i + 7);
        game   = sonus_test::Game::start(bank, c.port, error);
    }
    if (!game) std::printf("the game did not start: %s\n", error.c_str());
    ASSERT_TRUE(game != nullptr);
    c.project_dir = dir.path("project");
    c.tick_ms     = 10;
    game->play("Tone");
    SonusPlugin plugin(c, nullptr);
    SonusView   view(plugin);
    Tab t = build(view);
    EXPECT_TRUE(t.built.mentions("Master"));
    EXPECT_TRUE(t.built.mentions("Sfx"));
    EXPECT_EQ(t.built.meters, 3);
    EXPECT_TRUE(t.built.click(t.root, "接続"));
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (plugin.shown().state != LinkState::Online && std::chrono::steady_clock::now() < end) {
        game->step();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(plugin.shown().state == LinkState::Online);
    t = build(view);
    EXPECT_TRUE(t.built.mentions("接続済み"));
    // The first M is the master's; mute it through the tab.
    EXPECT_TRUE(t.built.click(t.root, "M"));
    EXPECT_TRUE(plugin.shown().buses[0].muted);
    t = build(view);
    EXPECT_TRUE(t.built.mentions("元に戻す (1)"));
    EXPECT_TRUE(t.built.click(t.root, "元に戻す (1)"));
    EXPECT_FALSE(plugin.shown().buses[0].muted);
    EXPECT_TRUE(t.built.click(t.root, "-6"));
    EXPECT_NEAR(plugin.shown().buses[0].fader_db, -6.0f, 1e-4);
    t = build(view);
    EXPECT_TRUE(t.built.mentions("-6.0 dB"));
    EXPECT_TRUE(t.built.click(t.root, "上書き (1)"));
    t = build(view);
    EXPECT_TRUE(t.built.mentions("Master"));
    EXPECT_TRUE(t.built.click(t.root, "全部戻す"));
    EXPECT_TRUE(plugin.shown().overrides.empty());
    plugin.disconnect();
}
