// The Studio's UI on Tela (spec/feature/tool/shell.md): what ViewBuilder declares, the tabs,
// one declaration on two screens (a native runtime and a browser page), /shell/ and /shell/ws,
// and the locked tabs of a page under a public name.
#include "fake_ws.h"
#include "views/assets_view.h"
#include "views/play_view.h"

#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/tela/browser_screens.h"
#include "ergo/studio/tela/screen_hub.h"
#include "ergo/studio/tela/shell_routes.h"
#include "ergo/studio/tela/studio_declaration.h"
#include "ergo/studio/tela/tela_emitter.h"

#include <gtest/gtest.h>

#include <tela/runtime.hpp>

#include <string>

using namespace ergo::studio;
namespace ts = ergo::studio::tela_shell;
using studio_test::FakeConn;
using ergo::common::jsonm::JsonValue;

namespace {

void log_nothing(const std::string&, const std::string&) {}

/// A plugin with its own view: a counter, a name field, a secret, a slider and a video.
class FormPlugin final : public host::IPlugin, public host::IPluginView {
public:
    explicit FormPlugin(std::string id = "form") : info_{std::move(id), "Form", "", ""} {}
    const host::PluginInfo& info() const override { return info_; }
    host::IPluginView*      view() override { return this; }
    uint64_t                view_revision() const override { return revision; }
    void                    build(ui::ViewBuilder& b) override {
        b.title("Form");
        ui::ViewBuilder r = b.row();
        r.label("clicks " + std::to_string(clicks));
        r.button("add", [this] { ++clicks; });
        b.label("hello " + name);
        b.text_field(name, "your name");
        b.secret_field(token, "token");
        b.slider(0.25f, [this](float f) { fraction = f; });
        b.label("broken", &b.theme().error);
        b.video("picture", "play:demo", 320, 180);
        b.name_next("key-Space");
        b.button("jump", {});
    }
    uint64_t    revision = 1;
    int         clicks   = 0;
    float       fraction = 0;
    std::string name, token = "abc";

private:
    host::PluginInfo info_;
};

class PlainPlugin final : public host::IPlugin {
public:
    const host::PluginInfo& info() const override { return info_; }

private:
    host::PluginInfo info_{"plain", "Plain", "no view", ""};
};

struct Fixture {
    explicit Fixture(std::string form_id = "form") {
        auto loaded = host::instantiate_plugins(
            {[form_id](const host::LogFn&) { return std::make_unique<FormPlugin>(form_id); },
             [](const host::LogFn&) { return std::make_unique<PlainPlugin>(); }},
            {}, &log_nothing);
        host = std::make_unique<host::PluginHost>(std::move(loaded), &log_nothing);
        form = static_cast<FormPlugin*>(host->plugins()[0]);
        decl = std::make_unique<ts::StudioDeclaration>(*host, ui::ViewLookup{});
    }
    std::unique_ptr<host::PluginHost>       host;
    FormPlugin*                             form = nullptr;
    std::unique_ptr<ts::StudioDeclaration>  decl;
};

const tela::Element* find(const tela::Document& d, const std::string& id) {
    for (const auto& e : d.elements()) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const tela::Element* find_kind(const tela::Document& d, tela::ElementKind k, const std::string& label = {}) {
    for (const auto& e : d.elements()) {
        if (e.kind == k && (label.empty() || e.label == label || e.value.text == label)) return &e;
    }
    return nullptr;
}

bool has_text(const tela::Runtime& r, const std::string& text) {
    for (const auto& p : r.elements()) {
        if (p.element.kind == tela::ElementKind::text && p.element.label == text) return true;
    }
    return false;
}

void show(tela::Runtime& r, int w = 1000, int h = 700) {
    tela::Viewport v;
    v.host_id  = "test";
    v.view_id  = "native";
    v.revision = 1;
    v.width    = w;
    v.height   = h;
    v.visible  = true;
    v.focused  = true;
    r.viewport(v);
}

net::HttpRequest request(const std::string& path, const std::string& host = "127.0.0.1:5170") {
    net::HttpRequest req;
    req.method          = "GET";
    req.path            = path;
    req.target          = path;
    req.headers["host"] = host;
    return req;
}

class NoHttp final : public net::IHttpHandler, public net::IWsAcceptor {
public:
    net::HttpResponse handle(const net::HttpRequest&) override { return net::HttpResponse::json(404, "{}"); }
    bool              accepts(const std::string&) const override { return false; }
    net::WsHandlers   accept(const std::string&, const net::HttpRequest&, std::shared_ptr<net::IWsConnection>) override {
        return {};
    }
};

std::string resize_message(int w, int h) {
    return "{\"type\":\"resize\",\"width\":" + std::to_string(w) + ",\"height\":" + std::to_string(h) + ",\"dpi\":1}";
}

} // namespace

TEST(StudioTela, ViewBuilderDeclaresTelaElementsWithPathIds) {
    Fixture f;
    f.decl->select_tab(1);
    ASSERT_TRUE(f.decl->refresh(0));
    const auto& doc = f.decl->document(ts::ScreenAccess::local);

    // Tab list and content are positioned scrolls side by side.
    const auto* tabs = find(doc, "tabs");
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->kind, tela::ElementKind::scroll);
    const auto* content = find(doc, "content/form");
    ASSERT_NE(content, nullptr);
    EXPECT_TRUE(content->layout.positioned);
    EXPECT_GT(content->layout.x, 0);
    const auto* tab = find(doc, "tabs/item1");
    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(tab->kind, tela::ElementKind::list_item);
    EXPECT_TRUE(tab->value.selected);

    // The path of node names is the id; a row's children get their natural width.
    const auto* row = find(doc, "content/form/row1");
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(row->layout.flow, tela::Flow::row);
    const auto* add = find(doc, "content/form/row1/button1");
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->kind, tela::ElementKind::button);
    EXPECT_GT(add->layout.width, 0);
    EXPECT_TRUE(static_cast<bool>(add->action));
    EXPECT_GT(find(doc, "content/form/row1/label0")->layout.width, 0);

    const auto* name = find_kind(doc, tela::ElementKind::text_field, "your name");
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->layout.width, 320);
    const auto* token = find_kind(doc, tela::ElementKind::text_field, "abc");
    ASSERT_NE(token, nullptr);
    EXPECT_TRUE(token->value.secret);
    const auto* slider = find_kind(doc, tela::ElementKind::slider);
    ASSERT_NE(slider, nullptr);
    EXPECT_FLOAT_EQ(slider->value.fraction, 0.25f);
    const auto* video = find_kind(doc, tela::ElementKind::video);
    ASSERT_NE(video, nullptr);
    EXPECT_EQ(video->value.stream, "play:demo");
    EXPECT_EQ(video->layout.height, 180);
    // A label in the theme's error colour is a status line with the bad tone.
    const auto* broken = find_kind(doc, tela::ElementKind::status, "broken");
    ASSERT_NE(broken, nullptr);
    EXPECT_EQ(broken->value.tone, tela::Tone::bad);
    // name_next gives the page script a findable id.
    EXPECT_NE(find(doc, "content/form/key-Space"), nullptr);
    // The title is bigger text.
    const auto* title = find(doc, "content/form/title0");
    ASSERT_NE(title, nullptr);
    EXPECT_GT(title->layout.text_scale, 1.0f);
}

TEST(StudioTela, TabsSwitchAndRebuildOnRevisionAndActions) {
    Fixture f;
    ASSERT_TRUE(f.decl->refresh(0));
    EXPECT_EQ(f.decl->tab_count(), 3u);
    EXPECT_NE(find(f.decl->document(ts::ScreenAccess::local), "content/overview"), nullptr);
    EXPECT_FALSE(f.decl->refresh(0));   // nothing changed
    const auto rev = f.decl->revision();

    // Activating a tab line selects that tab on the next refresh.
    find(f.decl->document(ts::ScreenAccess::local), "tabs/item1")->action();
    ASSERT_TRUE(f.decl->refresh(0));
    EXPECT_EQ(f.decl->active_tab(), 1u);
    EXPECT_GT(f.decl->revision(), rev);
    EXPECT_NE(find(f.decl->document(ts::ScreenAccess::local), "content/form"), nullptr);

    // A button's action runs the view's callback and rebuilds.
    find(f.decl->document(ts::ScreenAccess::local), "content/form/row1/button1")->action();
    EXPECT_EQ(f.form->clicks, 1);
    ASSERT_TRUE(f.decl->refresh(0));
    EXPECT_NE(find_kind(f.decl->document(ts::ScreenAccess::local), tela::ElementKind::text, "clicks 1"), nullptr);

    // The view's revision alone also rebuilds.
    ++f.form->revision;
    EXPECT_TRUE(f.decl->refresh(0));

    // A plugin without a view gets the generic tab.
    f.decl->select_tab(2);
    ASSERT_TRUE(f.decl->refresh(0));
    EXPECT_NE(find_kind(f.decl->document(ts::ScreenAccess::local), tela::ElementKind::text, "no view"), nullptr);
}

TEST(StudioTela, TwoScreensShareValuesButNotFocus) {
    Fixture f;
    f.decl->select_tab(1);
    ts::BrowserScreens pages(&log_nothing);
    ts::ScreenHub      hub(*f.decl, pages);
    tela::Runtime      native;
    show(native);
    hub.add(native, ts::ScreenAccess::local);

    auto conn = std::make_shared<FakeConn>(7);
    net::WsHandlers h = pages.open(conn, ts::ScreenAccess::local);
    ASSERT_TRUE(static_cast<bool>(h.on_text));
    hub.pump(0);
    EXPECT_EQ(pages.size(), 1u);
    h.on_text(resize_message(1000, 700));
    hub.pump(0);
    ASSERT_FALSE(conn->frames().empty());   // the page got its first frame

    // Type into the name field on the native screen.
    const auto* field = find_kind(f.decl->document(ts::ScreenAccess::local), tela::ElementKind::text_field, "your name");
    ASSERT_NE(field, nullptr);
    const std::string field_id = field->id;   // the declaration is replaced on the next pump
    ASSERT_TRUE(native.focus(field_id));
    native.text({1, "Ada"});
    EXPECT_EQ(f.form->name, "Ada");   // the value lives in the view
    hub.pump(0);
    EXPECT_TRUE(has_text(native, "hello Ada"));

    // The page shows the same value, without the native screen's focus.
    const auto frames = conn->frames();
    ASSERT_FALSE(frames.empty());
    const JsonValue& last = frames.back();
    EXPECT_NE(ergo::common::jsonm::serialize(last).find("hello Ada"), std::string::npos);
    ASSERT_NE(last.find("focus"), nullptr);
    EXPECT_TRUE(last.find("focus")->is_null());
    EXPECT_EQ(native.focused(), field_id);

    // A page that went away is dropped.
    h.on_close();
    hub.pump(0);
    EXPECT_EQ(pages.size(), 0u);
    hub.remove(native);
}

TEST(StudioTela, PageInputRunsTheDeclarationsCallbacks) {
    Fixture f;
    f.decl->select_tab(1);
    ts::BrowserScreens pages(&log_nothing);
    ts::ScreenHub      hub(*f.decl, pages);
    auto conn = std::make_shared<FakeConn>(3);
    net::WsHandlers h = pages.open(conn, ts::ScreenAccess::local);
    hub.pump(0);
    h.on_text(resize_message(1000, 700));
    hub.pump(0);

    // Where the page placed the "add" button: from its last frame.
    const JsonValue frame = conn->frames().back();
    const JsonValue* elements = frame.find("elements");
    ASSERT_NE(elements, nullptr);
    float x = -1, y = -1;
    for (const auto& e : *elements->a) {
        if (e.find("id") && e.find("id")->s == "content/form/row1/button1") {
            const auto& b = *e.find("bounds")->a;
            x = static_cast<float>(b[0].n + b[2].n / 2);
            y = static_cast<float>(b[1].n + b[3].n / 2);
        }
    }
    ASSERT_TRUE(x >= 0);
    const std::string at = ",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y);
    h.on_text("{\"type\":\"pointer\",\"phase\":\"down\",\"button\":\"primary\"" + at + "}");
    h.on_text("{\"type\":\"pointer\",\"phase\":\"up\",\"button\":\"primary\"" + at + "}");
    hub.pump(0);
    EXPECT_EQ(f.form->clicks, 1);
    hub.pump(0);
    EXPECT_NE(ergo::common::jsonm::serialize(conn->frames().back()).find("clicks 1"), std::string::npos);

    // A malformed message reaches nothing.
    h.on_text("{\"type\":\"nonsense\"}");
    hub.pump(0);
    EXPECT_EQ(f.form->clicks, 1);
}

TEST(StudioTela, PublishedPagesCannotOperateLoopbackOnlyTabs) {
    Fixture f("studio");   // the form stands in for the studio plugin's tab
    f.decl->select_tab(1);
    ASSERT_TRUE(f.decl->refresh(0));
    const auto& local     = f.decl->document(ts::ScreenAccess::local);
    const auto& published = f.decl->document(ts::ScreenAccess::published);
    EXPECT_TRUE(static_cast<bool>(find(local, "content/studio/row1/button1")->action));
    EXPECT_FALSE(static_cast<bool>(find(published, "content/studio/row1/button1")->action));
    EXPECT_FALSE(static_cast<bool>(find_kind(published, tela::ElementKind::text_field, "your name")->on_change));
    EXPECT_FALSE(static_cast<bool>(find_kind(published, tela::ElementKind::slider)->on_set));
    EXPECT_NE(find(published, "content/studio/locked"), nullptr);
    // The tab list still works there.
    EXPECT_TRUE(static_cast<bool>(find(published, "tabs/item0")->action));

    // Other tabs are the same for both.
    Fixture g;
    g.decl->select_tab(1);
    ASSERT_TRUE(g.decl->refresh(0));
    EXPECT_TRUE(static_cast<bool>(find(g.decl->document(ts::ScreenAccess::published), "content/form/row1/button1")->action));
    EXPECT_TRUE(ts::StudioDeclaration::loopback_only("lesson"));
    EXPECT_FALSE(ts::StudioDeclaration::loopback_only("assets"));
}

TEST(StudioTela, ShellRoutesServeThePageAndOpenPages) {
    NoHttp             next;
    ts::BrowserScreens pages(&log_nothing, 1);
    ts::ShellRoutes    routes(next, next, pages, "127.0.0.1");

    const auto page = routes.handle(request("/shell/"));
    EXPECT_EQ(page.status, 200);
    EXPECT_NE(page.body.find("/shell/tela.js"), std::string::npos);
    EXPECT_NE(page.headers.at("content-security-policy").find("frame-ancestors 'none'"), std::string::npos);
    const auto js = routes.handle(request("/shell/tela.js"));
    EXPECT_EQ(js.status, 200);
    EXPECT_NE(js.body.find("Tela"), std::string::npos);
    EXPECT_EQ(routes.handle(request("/shell/shell.js")).status, 200);
    EXPECT_EQ(routes.handle(request("/shell/nothing")).status, 404);
    auto post = request("/shell/");
    post.method = "POST";
    EXPECT_EQ(routes.handle(post).status, 405);
    EXPECT_EQ(routes.handle(request("/other/")).status, 404);   // the next handler

    EXPECT_TRUE(routes.accepts("shell"));
    EXPECT_FALSE(routes.accepts("other"));
    EXPECT_EQ(ts::screen_access(request("/shell/ws"), "127.0.0.1"), ts::ScreenAccess::local);
    EXPECT_EQ(ts::screen_access(request("/shell/ws", "studio.example.com"), "127.0.0.1"), ts::ScreenAccess::published);

    auto first  = std::make_shared<FakeConn>(1);
    auto second = std::make_shared<FakeConn>(2);
    EXPECT_TRUE(static_cast<bool>(routes.accept("shell", request("/shell/ws"), first).on_text));
    // One page at most here: the next is refused (1013).
    EXPECT_FALSE(static_cast<bool>(routes.accept("shell", request("/shell/ws"), second).on_text));
    EXPECT_FALSE(second->is_open());
    EXPECT_EQ(second->close_code(), 1013);
}

TEST(StudioTela, AssetAndPlayHelpers) {
    EXPECT_EQ(views::human_size(512), "512 B");
    EXPECT_EQ(views::human_size(2048), "2.0 KB");
    EXPECT_EQ(views::parent_dir("a/b/c.png"), "a/b");
    EXPECT_EQ(views::parent_dir("c.png"), "");
    EXPECT_EQ(views::PlayView::stream_key("demo"), "play:demo");
    EXPECT_EQ(views::play_state_text("running"), "実行中");
    EXPECT_EQ(views::play_state_text("idle"), "停止中");
}
