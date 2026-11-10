#include "fake_http.h"
#include "fake_ws.h"
#include "plugins/common/file_io.h"
#include "plugins/ui_layout/ui_layout_plugin.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::ui_layout;
using namespace studio_test;
namespace fs = std::filesystem;

TEST(StudioUiLayout, BridgeRefusesUntilConfigured) {
    auto http = std::make_shared<FakeHttp>();
    UiLayoutPlugin p({}, http, quiet());
    EXPECT_EQ(p.handle(req("POST", "{}"), "/api/bridge/patch").status, 503);
    EXPECT_TRUE(http->seen.empty());   // nothing was sent to a guessed endpoint
    EXPECT_EQ(p.handle(req("POST", R"({"patchUrl":"file:///x"})"), "/api/bridge/config").status, 400);

    ASSERT_EQ(p.handle(req("POST", R"({"patchUrl":"http://host:1/p","pullUrl":"http://host:1/s"})"), "/api/bridge/config").status, 200);
    http->routes["POST http://host:1/p"] = {204, ""};
    http->routes["GET http://host:1/s"]  = {200, R"({"root":1})"};
    Client ui = open(p, 1);
    const JsonValue patched = parse_json(p.handle(req("POST", R"({"x":1})"), "/api/bridge/patch").body);
    EXPECT_TRUE(field(patched, "ok")->as_bool());
    EXPECT_EQ(http->seen.at(0).body, R"({"x":1})");
    EXPECT_EQ(field(ui.conn->last("patch"), "payload")->as_string(), R"({"x":1})");
    EXPECT_EQ(p.handle(req("POST"), "/api/bridge/pull").status, 200);
    EXPECT_EQ(field(ui.conn->last("document"), "payload")->as_string(), R"({"root":1})");
    EXPECT_TRUE(is_http_url("https://a/b"));
    EXPECT_FALSE(is_http_url("http:///nohost"));
}

TEST(StudioUiLayout, FilesStayInsideTheRootWithTheRightExtension) {
    const fs::path root = fs::temp_directory_path() / "ergo_studio_ui_layout";
    fs::remove_all(root);
    UiLayoutPlugin p(root, std::make_shared<FakeHttp>(), quiet());
    EXPECT_EQ(p.handle(req("POST", R"({"path":"a/main.uilayout.json","json":"{}"})"), "/api/file/save").status, 200);
    EXPECT_EQ(p.handle(req("POST", R"({"path":"../x.uilayout.json","json":"{}"})"), "/api/file/save").status, 400);
    EXPECT_EQ(p.handle(req("POST", R"({"path":"a/main.json","json":"{}"})"), "/api/file/save").status, 400);
    net::HttpRequest r = req("GET");
    r.query = {{"path", "a/main.uilayout.json"}};
    EXPECT_EQ(field(parse_json(p.handle(r, "/api/file/open").body), "json")->as_string(), "{}");
    std::string err;
    write_text_file(root / "icon.svg", "<svg/>", err);
    r.query = {{"path", "icon.svg"}};
    const auto svg = p.handle(r, "/api/file/svg");
    EXPECT_EQ(svg.headers.at("content-type"), "image/svg+xml; charset=utf-8");
    UiLayoutPlugin unset({}, std::make_shared<FakeHttp>(), quiet());
    EXPECT_EQ(unset.handle(r, "/api/file/open").status, 503);
    fs::remove_all(root);
}
