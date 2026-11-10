#include "fake_ws.h"
#include "plugins/common/file_io.h"
#include "plugins/musa_terpsichore/command_api.h"
#include "plugins/musa_terpsichore/terpsichore_config.h"
#include "plugins/musa_terpsichore/terpsichore_plugin.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::musa_terpsichore;
using namespace studio_test;
namespace fs = std::filesystem;

namespace {

JsonValue parse(const std::string& s) {
    JsonValue v;
    ergo::common::jsonm::parse(s, v);
    return v;
}

net::HttpRequest at(const std::string& method, const std::string& path, const std::string& body = {}) {
    net::HttpRequest r = req(method, body);
    r.path = path;
    return r;
}

/// A port that was free a moment ago (bound on port 0, then released).
uint16_t free_port() {
    struct Nop final : net::IHttpHandler, net::IWsAcceptor {
        net::HttpResponse handle(const net::HttpRequest&) override { return {}; }
        bool accepts(const std::string&) const override { return false; }
        net::WsHandlers accept(const std::string&, const net::HttpRequest&, std::shared_ptr<net::IWsConnection>) override {
            return {};
        }
    } nop;
    net::Server s({"127.0.0.1", 0}, nop, nop, [](const std::string&, const std::string&) {});
    std::string err;
    if (!s.start(err)) return 0;
    const uint16_t p = s.port();
    s.stop();
    return p;
}

fs::path temp_file() {
    static std::atomic<int> n{0};
    const fs::path dir = fs::temp_directory_path() / ("ergo_studio_terp_" + std::to_string(++n));
    fs::remove_all(dir);
    return dir / "config.json";
}

} // namespace

TEST(StudioTerpsichoreConfig, RequiresPortAndValidatesFields) {
    std::string err;
    const auto ok = parse_config(parse(R"({"port":9000,"role":"watcher","token":"t"})"), err);
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok->port, 9000);
    EXPECT_EQ(ok->role, ServerRole::Watcher);
    EXPECT_EQ(ok->host, "127.0.0.1");
    EXPECT_TRUE(ok->requires_auth());
    EXPECT_FALSE(parse_config(parse(R"({"enabled":true})"), err).has_value());   // no built-in port
    EXPECT_FALSE(parse_config(parse(R"({"port":70000})"), err).has_value());
    EXPECT_FALSE(parse_config(parse(R"({"port":1,"role":"boss"})"), err).has_value());
    EXPECT_TRUE(config_path("").empty());

    const fs::path f = temp_file();
    EXPECT_FALSE(load_config(f).config.has_value());
    EXPECT_TRUE(load_config(f).error.empty());           // missing = not configured
    write_text_file(f, "{broken", err);
    EXPECT_FALSE(load_config(f).error.empty());          // broken = reported
    fs::remove_all(f.parent_path());
}

TEST(StudioTerpsichoreApi, AuthGuardsEveryPost) {
    TerpsichoreConfig cfg;
    cfg.port  = 1;
    cfg.token = "secret";
    CommandApi api(cfg);
    EXPECT_EQ(api.handle(at("GET", "/api/health")).status, 200);
    EXPECT_EQ(api.handle(at("POST", "/api/play-start")).status, 401);
    EXPECT_EQ(api.handle(at("POST", "/api/execute-command", R"({"command":"x"})")).status, 401);

    net::HttpRequest r = at("POST", "/api/play-start");
    r.headers["authorization"] = "Bearer secret";
    EXPECT_EQ(api.handle(r).status, 200);
    const JsonValue play = parse(api.handle(at("GET", "/api/play-status")).body);
    EXPECT_TRUE(field(play, "playing")->as_bool());
    EXPECT_EQ(field(api.status(), "request_count")->as_number(), 1.0);
}

TEST(StudioTerpsichoreApi, ExecuteCommandUsesTheHandler) {
    TerpsichoreConfig cfg;
    cfg.port = 1;
    CommandApi api(cfg);
    JsonValue r = parse(api.handle(at("POST", "/api/execute-command", R"({"command":"spawn","args":["a"]})")).body);
    EXPECT_EQ(field(*field(r, "data"), "note")->as_string(), "No command handler registered");
    EXPECT_EQ(api.handle(at("POST", "/api/execute-command", R"({"args":[]})")).status, 400);
    EXPECT_EQ(api.handle(at("POST", "/api/execute-command", R"({"command":"x","args":[1]})")).status, 400);

    api.set_command_handler([](const CommandRequest& c) {
        return CommandResult{c.command == "spawn", "ran " + c.command + " " + c.args.at(0), JsonValue::make_null()};
    });
    r = parse(api.handle(at("POST", "/api/execute-command", R"({"command":"spawn","args":["a"]})")).body);
    EXPECT_TRUE(field(r, "success")->as_bool());
    EXPECT_EQ(field(r, "message")->as_string(), "ran spawn a");
    EXPECT_EQ(field(r, "data"), nullptr);

    EXPECT_EQ(api.handle(at("POST", "/api/build", "{}")).status, 400);
    EXPECT_EQ(api.handle(at("POST", "/api/build", R"({"target":"win","output_path":"out"})")).status, 200);
    EXPECT_EQ(field(parse(api.handle(at("GET", "/api/build-status")).body), "output_path")->as_string(), "out");
}

TEST(StudioTerpsichorePlugin, NotConfiguredThenConfigureStartStop) {
    const fs::path f = temp_file();
    TerpsichorePlugin p(f, quiet());
    EXPECT_TRUE(field(p.snapshot(), "ok")->as_bool());          // unconfigured is normal
    EXPECT_FALSE(field(p.snapshot(), "configured")->as_bool());
    EXPECT_EQ(p.handle(req("POST"), "/api/start").status, 400);

    const uint16_t port = free_port();
    ASSERT_NE(port, 0);
    const std::string body = "{\"config\":{\"port\":" + std::to_string(port) + "}}";
    EXPECT_EQ(p.handle(req("POST", body), "/api/config").status, 200);
    EXPECT_TRUE(load_config(f).config.has_value());             // saved to disk

    ASSERT_EQ(p.handle(req("POST"), "/api/start").status, 200);
    EXPECT_TRUE(p.running());
    EXPECT_EQ(p.handle(req("POST"), "/api/start").status, 409);
    EXPECT_EQ(field(p.status(), "port")->as_number(), static_cast<double>(port));
    EXPECT_EQ(p.handle(req("POST"), "/api/stop").status, 200);
    EXPECT_FALSE(p.running());
    ASSERT_EQ(p.handle(req("POST"), "/api/start").status, 200);  // the port is really released
    p.stop();
    fs::remove_all(f.parent_path());
}

TEST(StudioTerpsichorePlugin, BrokenConfigIsUnhealthy) {
    const fs::path f = temp_file();
    std::string err;
    write_text_file(f, R"({"port":"x"})", err);
    TerpsichorePlugin p(f, quiet());
    EXPECT_FALSE(field(p.snapshot(), "ok")->as_bool());
    EXPECT_NE(field(p.snapshot(), "err"), nullptr);
    fs::remove_all(f.parent_path());
}
