#include "fake_http.h"
#include "views/lesson_view.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <thread>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::lesson;
using namespace studio_test;

namespace {

const char* const kLink  = "http://127.0.0.1:4410/v1/tool#7K2M9QX4TB";
const char* const kToken = "0123456789abcdef0123456789abcdef";
const char* const kPair  = "POST http://127.0.0.1:4410/v1/tool/pair";
const char* const kState = "GET http://127.0.0.1:4410/v1/tool/state";

std::string pair_body(const char* role) {
    return std::string(R"({"token":")") + kToken + R"(","role":")" + role + R"(","class":{"title":"Action"},"poll_ms":10000})";
}

class FakeClipboard final : public platform::IClipboard {
public:
    platform::ClipboardText next;
    platform::ClipboardText read_text() override { return next; }
};

class NoVars final : public IGameVariables {
public:
    std::optional<GameVariable> find(const std::string&) const override { return std::nullopt; }
    bool request_set(const std::string&, const JsonValue&) override { return false; }
};

/// The tab as built: its texts and its buttons (in build order).
struct Built {
    std::vector<std::string>      texts;
    std::vector<ui::ClickBinding> clicks;

    bool has(const std::string& text) const {
        for (const auto& t : texts) {
            if (t == text) return true;
        }
        return false;
    }
    bool mentions(const std::string& part) const {
        for (const auto& t : texts) {
            if (t.find(part) != std::string::npos) return true;
        }
        return false;
    }
};

void collect(const ergo::ui_kit::Node& n, std::vector<std::string>& out) {
    if (n.kind == ergo::ui_kit::NodeKind::Text) out.push_back(n.text);
    for (const auto& c : n.children) collect(c, out);
}

Built build(views::LessonView& view) {
    Built                   out;
    ergo::ui_kit::Node      root;
    ui::FixedAdvanceMeasure measure;
    ui::Theme               theme;
    ui::ViewBuilder         b(root, "Root", measure, theme, out.clicks);
    view.build(b);
    collect(root, out.texts);
    return out;
}

bool wait_until(const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

struct Fixture {
    Fixture()
        : http(std::make_shared<FakeHttp>()),
          clipboard(std::make_shared<FakeClipboard>()),
          plugin(http, std::make_shared<NoVars>(), [](const std::string&, const std::string&) {}),
          view(plugin, clipboard) {}
    std::shared_ptr<FakeHttp>      http;
    std::shared_ptr<FakeClipboard> clipboard;
    LessonPlugin                   plugin;
    views::LessonView              view;
};

} // namespace

TEST(StudioLessonView, UnlinkedShowsTheButtonsAndWhyAPasteWasRejected) {
    Fixture f;
    Built b = build(f.view);
    EXPECT_TRUE(b.has("授業"));
    EXPECT_TRUE(b.has("未接続"));
    ASSERT_EQ(b.clicks.size(), 2u);   // 貼り付けてつなぐ / 切る
    EXPECT_TRUE(b.has("貼り付けてつなぐ"));
    EXPECT_TRUE(b.has("切る"));

    uint64_t rev = f.view.view_revision();
    f.clipboard->next = {false, {}, "not supported on this platform"};
    b.clicks[0].fn();
    EXPECT_GT(f.view.view_revision(), rev);
    b = build(f.view);
    EXPECT_TRUE(b.mentions("クリップボードを読めませんでした"));
    EXPECT_TRUE(b.mentions("not supported on this platform"));

    rev = f.view.view_revision();
    f.clipboard->next = {true, "https://lesson.example.org/v1/tool#SECRETC0D", {}};   // 9 symbols
    b.clicks[0].fn();
    EXPECT_GT(f.view.view_revision(), rev);
    b = build(f.view);
    EXPECT_FALSE(b.mentions("クリップボードを読めませんでした"));
    EXPECT_TRUE(b.mentions("コードは 10 文字"));
    EXPECT_FALSE(b.mentions("SECRETC0D"));   // the pasted text is never shown
    EXPECT_FALSE(b.mentions("lesson.example.org"));
    EXPECT_TRUE(f.http->seen.empty());
}

TEST(StudioLessonView, AStudentSeesTheAssignmentWithoutAnySecret) {
    Fixture f;
    f.http->routes[kPair]  = {200, pair_body("student")};
    f.http->routes[kState] = {200, R"({"role":"student","class":{"title":"Action"},"features":{},
      "assignment":{"id":"a","title":"Restore the feel","active":true,"steps":[
        {"id":"s0","label":"Explain","done":false},
        {"id":"s1","label":"Add hit stop","done":false,"check":{"variable":"v","op":"gt","value":0}},
        {"id":"s2","label":"Shake","done":true,"check":{"variable":"w","op":"eq","value":true}}]}})"};
    f.clipboard->next = {true, std::string(kLink) + "\n", {}};
    build(f.view).clicks[0].fn();
    ASSERT_TRUE(wait_until([&] { return f.plugin.shown().session.assignment.has_value(); }));

    const Built b = build(f.view);
    EXPECT_TRUE(b.has("接続済み (生徒)"));
    EXPECT_TRUE(b.has("127.0.0.1"));
    EXPECT_TRUE(b.has("Action"));
    EXPECT_TRUE(b.has("課題: Restore the feel"));
    EXPECT_TRUE(b.has("・ Explain"));
    EXPECT_TRUE(b.has("・ Add hit stop  [自動]"));
    EXPECT_TRUE(b.has("✓ Shake  [自動]"));
    EXPECT_FALSE(b.mentions(kToken));
    EXPECT_FALSE(b.mentions("7K2M9QX4TB"));
    EXPECT_FALSE(b.mentions("4410"));   // only the host

    b.clicks[1].fn();   // 切る
    ASSERT_TRUE(wait_until([&] { return !f.plugin.shown().session.linked; }));
    const Built after = build(f.view);
    EXPECT_TRUE(after.has("未接続"));
    EXPECT_TRUE(after.has("切りました"));
    EXPECT_FALSE(after.mentions("課題"));
}

TEST(StudioLessonView, APresenterSeesWhichFeaturesWaitForTheGame) {
    Fixture f;
    f.http->routes[kPair]  = {200, pair_body("presenter")};
    f.http->routes[kState] = {200, R"({"role":"presenter","class":{"title":"Action"},
      "features":{"hit_stop":"feel.hit_stop","knockback":"feel.knockback"},"assignment":null,
      "demo":{"seq":1,"states":{"hit_stop":true,"slow_motion":false}}})"};
    f.clipboard->next = {true, kLink, {}};
    build(f.view).clicks[0].fn();
    ASSERT_TRUE(wait_until([&] { return f.plugin.shown().session.demo.size() == 3; }));

    const Built b = build(f.view);
    EXPECT_TRUE(b.has("接続済み (実演)"));
    EXPECT_TRUE(b.mentions("hit_stop  [feel.hit_stop]  ON  未反映"));
    EXPECT_TRUE(b.has("knockback  [feel.knockback]  —"));
    EXPECT_TRUE(b.has("slow_motion  OFF  対応する変数がありません"));
}

TEST(StudioLessonView, FailuresAreShownByClass) {
    Fixture f;
    f.clipboard->next = {true, kLink, {}};
    build(f.view).clicks[0].fn();   // no route: transport failure
    ASSERT_TRUE(wait_until([&] { return f.plugin.shown().session.failure.failed(); }));
    EXPECT_TRUE(build(f.view).mentions("接続に失敗しました: サーバにつながりません"));

    f.http->routes[kPair] = {404, R"({"error":"link_not_found"})"};
    build(f.view).clicks[0].fn();
    ASSERT_TRUE(wait_until([&] { return f.plugin.shown().session.failure.status == 404; }));
    Built b = build(f.view);
    EXPECT_TRUE(b.mentions("HTTP 404 (link_not_found)"));
    EXPECT_TRUE(b.has("未接続"));

    f.http->routes[kPair]  = {200, pair_body("student")};
    f.http->routes[kState] = {401, R"({"error":"tool_unlinked"})"};
    build(f.view).clicks[0].fn();
    ASSERT_TRUE(wait_until([&] { return f.plugin.shown().session.end == LinkEnd::Unlinked; }));
    b = build(f.view);
    EXPECT_TRUE(b.has("接続が切れました。授業ページで接続文字列を取り直してください"));
    EXPECT_TRUE(b.has("未接続"));
}
