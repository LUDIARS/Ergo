#include "ergo/studio/host/health_report.h"
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/host/plugin_loader.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace ergo::studio;
using namespace ergo::studio::host;
using ergo::common::jsonm::JsonValue;

namespace {

/// Configurable fake: http / socket / health are present only when asked for.
class FakePlugin final : public IPlugin, public IPluginHttp, public IPluginHealth, public IPluginSocket {
public:
    explicit FakePlugin(PluginInfo info) : info_(std::move(info)) {}
    const PluginInfo& info() const override { return info_; }
    IPluginHttp*   http() override { return with_http ? this : nullptr; }
    IPluginHealth* health() override { return health_mode == Health::None ? nullptr : this; }
    IPluginSocket* socket() override { return with_socket ? this : nullptr; }

    net::HttpResponse handle(const net::HttpRequest&, const std::string& sub) override {
        return net::HttpResponse::json(200, "{\"sub\":\"" + sub + "\"}");
    }
    JsonValue snapshot() const override {
        if (health_mode == Health::Throws) throw std::runtime_error("boom");
        if (health_mode == Health::NoOk) return JsonValue::make_object();
        JsonValue h = JsonValue::make_object();
        h.set("ok", JsonValue::make_bool(health_mode == Health::Ok));
        h.set("clients", JsonValue::make_number(2));
        return h;
    }
    net::WsHandlers on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection>) override { return {}; }

    enum class Health { None, Ok, Bad, Throws, NoOk };
    bool   with_http   = false;
    bool   with_socket = false;
    Health health_mode = Health::None;

private:
    PluginInfo info_;
};

/// Literals only: two pointers keep the lambda inside std::function's own
/// storage (a heap-held lambda reads as a leak to the static analyzer).
PluginFactory make(const char* id, const char* title = "T") {
    return [id, title](const LogFn&) -> std::unique_ptr<IPlugin> {
        return std::make_unique<FakePlugin>(PluginInfo{id, title, "", ""});
    };
}

/// `cfg` is any callable taking FakePlugin& (kept as its own type: wrapping it in
/// std::function first trips the static analyzer's stack-escape check).
template <class Configure>
PluginFactory make_with(Configure cfg, const std::string& id) {
    return [cfg, id](const LogFn&) -> std::unique_ptr<IPlugin> {
        auto p = std::make_unique<FakePlugin>(PluginInfo{id, "T", "d", "i"});
        cfg(*p);
        return p;
    };
}

const LogFn kQuiet = [](const std::string&, const std::string&) {};

net::HttpRequest get(const std::string& path) {
    net::HttpRequest r;
    r.method = "GET";
    r.path   = path;
    return r;
}

/// mini-gtest has no EXPECT_THROW.
bool builtin_load_throws(const std::vector<PluginFactory>& builtins) {
    try {
        instantiate_plugins(builtins, {}, kQuiet);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

} // namespace

TEST(StudioHostId, PatternAndReserved) {
    EXPECT_TRUE(is_valid_plugin_id("variable"));
    EXPECT_TRUE(is_valid_plugin_id("musa_thaleia"));
    EXPECT_TRUE(is_valid_plugin_id("vfx-suggest"));
    EXPECT_TRUE(is_valid_plugin_id("0x"));
    EXPECT_FALSE(is_valid_plugin_id(""));
    EXPECT_FALSE(is_valid_plugin_id("_x"));
    EXPECT_FALSE(is_valid_plugin_id("Var"));
    EXPECT_FALSE(is_valid_plugin_id("a/b"));
    EXPECT_TRUE(is_reserved_plugin_id("api"));
    EXPECT_TRUE(is_reserved_plugin_id("shell"));
    EXPECT_FALSE(is_reserved_plugin_id("log"));
}

TEST(StudioHostLoader, BuiltinProblemsThrow) {
    EXPECT_TRUE(builtin_load_throws({make("api")}));
    EXPECT_TRUE(builtin_load_throws({make("Bad")}));
    EXPECT_TRUE(builtin_load_throws({make("a", "")}));
    EXPECT_TRUE(builtin_load_throws({make("a"), make("a")}));
    EXPECT_TRUE(builtin_load_throws(
        {[](const LogFn&) -> std::unique_ptr<IPlugin> { throw std::runtime_error("x"); }}));
    EXPECT_FALSE(builtin_load_throws({make("a"), make("b")}));
}

TEST(StudioHostLoader, ExternalProblemsAreSkippedAndRecorded) {
    std::vector<std::string> logged;
    const LogFn log = [&](const std::string&, const std::string& m) { logged.push_back(m); };
    const auto r = instantiate_plugins(
        {make("variable")},
        {{make("variable"), "pack/a.dll"},
         {[](const LogFn&) -> std::unique_ptr<IPlugin> { return nullptr; }, "pack/b.dll"},
         {make("scene"), "pack/c.dll"}},
        log);
    ASSERT_EQ(r.plugins.size(), 2u);
    EXPECT_EQ(r.plugins[0].plugin->info().id, "variable");
    EXPECT_TRUE(r.plugins[0].builtin);
    EXPECT_EQ(r.plugins[1].source, "pack/c.dll");
    ASSERT_EQ(r.rejected.size(), 2u);
    EXPECT_EQ(r.rejected[0].id, "variable");
    EXPECT_NE(r.rejected[0].reason.find("already provided by built-in"), std::string::npos);
    EXPECT_EQ(r.rejected[1].id, "");
    EXPECT_EQ(logged.size(), 2u);
}

TEST(StudioHostHealth, NormalisesPluginHealth) {
    FakePlugin none({"a", "A", "", ""});
    FakePlugin bad({"b", "B", "", ""});
    bad.health_mode = FakePlugin::Health::Throws;
    FakePlugin no_ok({"c", "C", "", ""});
    no_ok.health_mode = FakePlugin::Health::NoOk;

    EXPECT_TRUE(plugin_health(none).find("ok")->as_bool());
    const JsonValue h = plugin_health(bad);
    EXPECT_FALSE(h.find("ok")->as_bool());
    EXPECT_NE(h.find("err")->as_string().find("boom"), std::string::npos);
    EXPECT_EQ(h.find("id")->as_string(), "b");
    EXPECT_FALSE(plugin_health(no_ok).find("ok")->as_bool());
}

TEST(StudioHostHealth, ReportIsFalseWhenAnythingRejected) {
    FakePlugin ok({"a", "A", "", ""});
    ok.health_mode = FakePlugin::Health::Ok;
    JsonValue root;
    ASSERT_TRUE(ergo::common::jsonm::parse(health_report({&ok}, {}), root));
    EXPECT_TRUE(root.find("ok")->as_bool());
    EXPECT_EQ(root.find("plugins")->a->at(0).find("clients")->as_number(), 2.0);

    ASSERT_TRUE(ergo::common::jsonm::parse(health_report({&ok}, {{"x", "pack/x.dll", "bad"}}), root));
    EXPECT_FALSE(root.find("ok")->as_bool());
    const JsonValue& rej = root.find("rejected")->a->at(0);
    EXPECT_EQ(rej.find("source")->as_string(), "pack/x.dll");
    EXPECT_EQ(rej.find("err")->as_string(), "bad");
}

TEST(StudioHostRouter, RoutesApiAndPluginPaths) {
    auto loaded = instantiate_plugins(
        {make_with([](FakePlugin& p) { p.with_http = true; p.with_socket = true; }, "variable"),
         make_with([](FakePlugin&) {}, "log")},
        {}, kQuiet);
    PluginHost host(std::move(loaded), kQuiet);

    EXPECT_EQ(host.handle(get("/variable/api/x")).body, "{\"sub\":\"/api/x\"}");
    EXPECT_EQ(host.handle(get("/variable")).body, "{\"sub\":\"/\"}");
    EXPECT_EQ(host.handle(get("/log/api/x")).status, 404);      // no IPluginHttp
    EXPECT_EQ(host.handle(get("/nope/api")).status, 404);
    EXPECT_EQ(host.handle(get("/api/health")).status, 200);

    net::HttpRequest post = get("/api/health");
    post.method = "POST";
    EXPECT_EQ(host.handle(post).status, 405);

    JsonValue list;
    ASSERT_TRUE(ergo::common::jsonm::parse(host.handle(get("/api/plugins")).body, list));
    const auto& arr = *list.find("plugins")->a;
    ASSERT_EQ(arr.size(), 2u);
    EXPECT_EQ(arr[0].find("ws")->as_string(), "/variable/ws");
    EXPECT_EQ(arr[1].find("ws"), nullptr);

    EXPECT_TRUE(host.accepts("variable"));
    EXPECT_FALSE(host.accepts("log"));
    EXPECT_FALSE(host.accepts("nope"));
}
