// Native tabs: text input through StudioApp, wrapping, the in-process plugin
// API, and the views drawing what a plugin's routes answer (GPU free).
#include "views/plugin_api.h"
#include "views/view_registry.h"

#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/ui/studio_app.h"
#include "ergo/studio/ui/view_builder.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace ergo::studio;
using namespace ergo::studio::ui;

namespace {

/// A plugin with a form: a text field (Enter submits), a secret field and a text area.
class FormPlugin final : public host::IPlugin, public host::IPluginView {
public:
    const host::PluginInfo& info() const override { return info_; }
    host::IPluginView*      view() override { return this; }
    uint64_t view_revision() const override { return submits; }
    void     build(ViewBuilder& b) override {
        b.title("Form");
        b.text_field(name, "name", 300, [this] { ++submits; });
        b.secret_field(secret, "token", 300);
        b.text_area(notes, 300, 3);
    }
    std::string name, secret, notes;
    uint64_t    submits = 0;

private:
    host::PluginInfo info_{"form", "Form", "", ""};
};

/// Answers GET /api/health and GET /api/items?q=..., counts calls.
class ApiPlugin final : public host::IPlugin, public host::IPluginHttp {
public:
    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    net::HttpResponse       handle(const net::HttpRequest& req, const std::string& sub) override {
        ++calls;
        last_sub   = sub;
        last_query = req.query.count("q") ? req.query.at("q") : "";
        last_host  = req.header("host").value_or("");
        if (sub == "/api/health") return net::HttpResponse::json(200, R"({"ok":true,"clients":3})");
        if (sub == "/api/items") return net::HttpResponse::json(200, R"({"items":["alpha","beta"]})");
        return net::HttpResponse::json(404, R"({"error":"nope"})");
    }
    std::atomic<int> calls{0};
    std::string      last_sub, last_query, last_host;

private:
    host::PluginInfo info_{"demo", "Api Plugin", "answers", ""};
};

const ergo::ui_kit::DrawCmd* find_text(const std::vector<ergo::ui_kit::DrawCmd>& list, const std::string& text) {
    for (const auto& c : list) {
        if (c.kind == ergo::ui_kit::DrawKind::Text && c.text == text) return &c;
    }
    return nullptr;
}

bool has_text_containing(const std::vector<ergo::ui_kit::DrawCmd>& list, const std::string& part) {
    for (const auto& c : list) {
        if (c.kind == ergo::ui_kit::DrawKind::Text && c.text.find(part) != std::string::npos) return true;
    }
    return false;
}

void click_at(StudioApp& app, float x, float y) {
    app.frame({x, y, false}, 0.016f);
    app.frame({x, y, true}, 0.016f);
    app.frame({x, y, false}, 0.016f);
    app.frame({x, y, false}, 0.016f);
}

void click_text(StudioApp& app, const std::string& text) {
    const auto* t = find_text(app.draw_list(), text);
    ASSERT_NE(t, nullptr);
    const auto r = t->rect;
    click_at(app, r.x + r.w / 2, r.y + r.h / 2);
}

void type(StudioApp& app, const std::string& text, std::vector<Key> keys = {}) {
    InputState in;
    in.text = text;
    in.keys = std::move(keys);
    app.frame(in, 0.016f);
    app.frame({}, 0.016f);
}

std::unique_ptr<host::PluginHost> host_with(std::vector<host::PluginFactory> f) {
    auto loaded = host::instantiate_plugins(std::move(f), {}, [](const std::string&, const std::string&) {});
    return std::make_unique<host::PluginHost>(std::move(loaded), [](const std::string&, const std::string&) {});
}

/// Run frames until `pred` holds (the views answer on a worker thread).
template <class Pred>
bool frames_until(StudioApp& app, Pred pred) {
    for (int i = 0; i < 200; ++i) {
        app.frame({}, 0.016f);
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

} // namespace

TEST(StudioViews, TextFieldTakesFocusTextAndKeys) {
    FormPlugin* form = nullptr;
    auto host = host_with({[&form](const host::LogFn&) {
        auto p = std::make_unique<FormPlugin>();
        form   = p.get();
        return p;
    }});
    FixedAdvanceMeasure measure;
    StudioApp app(*host, measure);
    app.set_viewport(1000, 700);
    app.frame({}, 0.016f);
    click_text(app, "Form");   // the tab
    ASSERT_EQ(app.active_tab(), 1u);

    type(app, "ignored");   // nothing focused yet
    EXPECT_EQ(form->name, "");

    click_text(app, "name");   // the placeholder sits inside the field
    EXPECT_FALSE(app.focused_field().empty());
    type(app, "ab\xE3\x81\x82");   // ab + あ
    type(app, "", {Key::Backspace});
    EXPECT_EQ(form->name, "ab");   // a whole code point went
    EXPECT_NE(find_text(app.draw_list(), "ab|"), nullptr);   // caret while focused
    type(app, "\tc\r");                                     // control characters are dropped
    EXPECT_EQ(form->name, "abc");
    type(app, "", {Key::Enter});
    EXPECT_EQ(form->submits, 1u);

    click_text(app, "token");
    type(app, "s3cr");
    EXPECT_EQ(form->secret, "s3cr");
    EXPECT_FALSE(has_text_containing(app.draw_list(), "s3cr"));   // shown as dots only

    type(app, "", {Key::Escape});
    EXPECT_TRUE(app.focused_field().empty());
}

TEST(StudioViews, TextAreaKeepsNewlinesAndPressElsewhereDropsFocus) {
    FormPlugin* form = nullptr;
    auto host = host_with({[&form](const host::LogFn&) {
        auto p = std::make_unique<FormPlugin>();
        form   = p.get();
        return p;
    }});
    FixedAdvanceMeasure measure;
    StudioApp app(*host, measure);
    app.set_viewport(1000, 700);
    app.select_tab(1);
    app.frame({}, 0.016f);
    // The text area is the third field: find it below the secret field's placeholder.
    const auto* tok = find_text(app.draw_list(), "token");
    ASSERT_NE(tok, nullptr);
    click_at(app, tok->rect.x + 5, tok->rect.y + tok->rect.h + 30);
    ASSERT_FALSE(app.focused_field().empty());
    type(app, "one", {Key::Enter});
    type(app, "two");
    EXPECT_EQ(form->notes, "one\ntwo");
    click_at(app, 900, 650);   // empty space
    EXPECT_TRUE(app.focused_field().empty());
}

TEST(StudioViews, WrapAndFit) {
    FixedAdvanceMeasure m(0.5f);   // 10 px per code point at size 20
    const auto lines = wrap_text("hello world again", 120, 20, m);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "hello world ");
    EXPECT_EQ(lines[1], "again");
    const auto cjk = wrap_text("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88", 25, 20, m);   // あいうえ, 2 a line
    ASSERT_EQ(cjk.size(), 2u);
    EXPECT_EQ(cjk[0], "\xE3\x81\x82\xE3\x81\x84");
    EXPECT_EQ(wrap_text("a\nb", 100, 20, m).size(), 2u);
    EXPECT_EQ(fit_text("abcdefghij", 50, 20, m), "abcd\xE2\x80\xA6");
    EXPECT_EQ(fit_tail("abcdefghij", 50, 20, m), "fghij");
}

TEST(StudioViews, RequestsReachTheRoutesAsLoopbackJson) {
    const auto r = views::make_request("studio", "POST", "/api/tasks?q=a%20b&x=1", "{}");
    EXPECT_EQ(r.path, "/studio/api/tasks");
    EXPECT_EQ(r.query.at("q"), "a b");
    EXPECT_EQ(r.query.at("x"), "1");
    EXPECT_EQ(r.header("host").value_or(""), "127.0.0.1");
    EXPECT_EQ(r.header("content-type").value_or(""), "application/json");
    EXPECT_EQ(views::url_encode("a b/\xE3\x81\x82"), "a%20b%2F%E3%81%82");

    ApiPlugin p;
    views::PluginApi api(p);
    const auto res = api.call_now("GET", "/api/items?q=x", "");
    EXPECT_TRUE(res.ok());
    EXPECT_EQ(p.last_sub, "/api/items");
    EXPECT_EQ(p.last_query, "x");
    const auto miss = api.call_now("GET", "/api/none", "");
    EXPECT_EQ(miss.status, 404);
    EXPECT_EQ(miss.error_text(), "nope");
}

TEST(StudioViews, ExplorerShowsHealthAndAnswers) {
    ApiPlugin* plugin = nullptr;
    auto host = host_with({[&plugin](const host::LogFn&) {
        auto p = std::make_unique<ApiPlugin>();
        plugin = p.get();
        return p;
    }});
    views::ViewRegistry reg;
    FixedAdvanceMeasure measure;
    StudioApp app(*host, measure, Theme{}, reg.lookup());
    app.set_viewport(1200, 900);
    app.select_tab(1);
    ASSERT_TRUE(frames_until(app, [&] { return find_text(app.draw_list(), "3") != nullptr; }));   // clients: 3
    EXPECT_NE(find_text(app.draw_list(), "Api Plugin"), nullptr);

    click_text(app, "/api/health");   // a known-endpoint button runs the request
    ASSERT_TRUE(frames_until(app, [&] { return find_text(app.draw_list(), "HTTP 200") != nullptr; }));
}

TEST(StudioViews, EveryBuiltInIdGetsAView) {
    ApiPlugin p;
    EXPECT_NE(views::make_view(p), nullptr);
}
