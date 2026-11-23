// The Studio in a browser (spec/feature/tool/web.md): the public-host guard,
// the profile's asset folders, the read-only asset list and preview, and the
// web plugin's page and config.
#include "plugins/assets/asset_catalog.h"
#include "plugins/assets/assets_plugin.h"
#include "plugins/common/json_parse.h"
#include "plugins/common/workspace_profiles.h"
#include "plugins/web/web_plugin.h"

#include "ergo/studio/net/request_guard.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
namespace fs = std::filesystem;

namespace {

fs::path fresh_dir(const std::string& tag) {
    static std::atomic<int> n{0};
    const fs::path dir = fs::temp_directory_path() / ("ergo-web-" + tag + "-" + std::to_string(++n));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

net::HttpRequest get(const std::string& path, std::map<std::string, std::string> query = {}) {
    net::HttpRequest r;
    r.method = "GET";
    r.path = path;
    r.target = path;
    r.query = std::move(query);
    r.headers["host"] = "127.0.0.1";
    return r;
}

const net::GuardPolicy kPolicy{"127.0.0.1", {"studio.example.org"}};

} // namespace

// ---- guard -----------------------------------------------------------------

TEST(WebGuard, PublicHostsAreParsedLowerCasedAndWithoutPorts) {
    const auto hosts = net::parse_public_hosts(" Studio.Example.org , ,bad host,with.port:8443,play.example.org");
    ASSERT_EQ(hosts.size(), 2u);
    EXPECT_EQ(hosts[0], "studio.example.org");
    EXPECT_EQ(hosts[1], "play.example.org");
}

TEST(WebGuard, APublicHostNeedsTheAccessAssertion) {
    EXPECT_TRUE(net::cross_site_problem(kPolicy, std::string("studio.example.org"), std::nullopt, false).has_value());
    EXPECT_FALSE(net::cross_site_problem(kPolicy, std::string("studio.example.org"), std::nullopt, true).has_value());
    EXPECT_FALSE(net::cross_site_problem(kPolicy, std::string("studio.example.org"),
                                         std::string("https://studio.example.org"), true).has_value());
}

TEST(WebGuard, APublicHostStillRefusesOtherOrigins) {
    EXPECT_TRUE(net::cross_site_problem(kPolicy, std::string("studio.example.org"),
                                        std::string("https://evil.example"), true).has_value());
    EXPECT_TRUE(net::ws_handshake_problem(kPolicy, std::string("studio.example.org"),
                                          std::string("https://evil.example"), true).has_value());
}

TEST(WebGuard, UnlistedNamesAndLoopbackKeepTheOldRules) {
    // Not listed: refused even with the header (a rebinding page cannot add it, but the name is not ours).
    EXPECT_TRUE(net::cross_site_problem(kPolicy, std::string("other.example.org"), std::nullopt, true).has_value());
    EXPECT_FALSE(net::cross_site_problem(kPolicy, std::string("127.0.0.1:5170"), std::string("http://127.0.0.1:5170"), false).has_value());
    // A listed name on a non-default port is not the published name.
    EXPECT_TRUE(net::cross_site_problem(kPolicy, std::string("studio.example.org:8443"), std::nullopt, true).has_value());
}

// ---- profile assets ----------------------------------------------------------

TEST(WebAssets, ProfileReadsAssetFoldersRelativeToTheRoot) {
    std::string err;
    const auto p = parse_workspace_profile(
        R"({"name":"amicii","root":"D:/LUDIARS/Amicii","assets":[{"name":"Models","path":"games/models"},{"path":"var"}]})",
        "a.studio.json", nullptr, err);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->assets.size(), 2u);
    EXPECT_EQ(p->assets[0].name, "Models");
    EXPECT_EQ(p->assets[0].path, fs::path("D:/LUDIARS/Amicii/games/models").lexically_normal());
    EXPECT_EQ(p->assets[1].name, "var");
}

TEST(WebAssets, AnAssetEntryWithoutPathIsAnError) {
    std::string err;
    EXPECT_FALSE(parse_workspace_profile(R"({"assets":[{"name":"x"}]})", "a.json", nullptr, err).has_value());
    EXPECT_NE(err.find("path"), std::string::npos);
}

TEST(WebAssets, KindsAndPreviews) {
    EXPECT_EQ(assets::kind_of("a/hero.GLB"), "model");
    EXPECT_EQ(assets::kind_of("t.png"), "texture");
    EXPECT_EQ(assets::kind_of("stage.scene.json"), "scene");
    EXPECT_EQ(assets::kind_of("x.balance"), "data");
    EXPECT_EQ(assets::kind_of("noext"), "other");
    EXPECT_TRUE(assets::preview_type("a.png").has_value());
    EXPECT_FALSE(assets::preview_type("a.svg").has_value());   // can carry script
    EXPECT_FALSE(assets::preview_type("a.glb").has_value());
}

TEST(WebAssets, ListsOneLevelFoldersFirstAndStaysInsideTheRoot) {
    const fs::path root = fresh_dir("list");
    write(root / "b.png", "png");
    write(root / "a.glb", "glb");
    write(root / "models" / "x.fbx", "fbx");
    write(root / ".git" / "HEAD", "ref");
    std::string err;
    const auto l = assets::list_folder(root, "", err);
    ASSERT_TRUE(l.has_value());
    ASSERT_EQ(l->entries.size(), 3u);   // .git is hidden
    EXPECT_EQ(l->entries[0].name, "models");
    EXPECT_TRUE(l->entries[0].directory);
    EXPECT_EQ(l->entries[1].name, "a.glb");
    EXPECT_EQ(l->entries[2].path, "b.png");
    const auto sub = assets::list_folder(root, "models", err);
    ASSERT_TRUE(sub.has_value());
    ASSERT_EQ(sub->entries.size(), 1u);
    EXPECT_EQ(sub->entries[0].path, "models/x.fbx");
    EXPECT_FALSE(assets::list_folder(root, "../", err).has_value());
    EXPECT_FALSE(assets::list_folder(root, "b.png", err).has_value());   // not a folder
}

TEST(WebAssets, PluginListsAndPreviewsFromAProfile) {
    const fs::path home = fresh_dir("home");
    const fs::path game = fresh_dir("game");
    write(game / "tex" / "sky.png", "PNGDATA");
    write(game / "tex" / "hero.glb", "GLB");
    write(home / "profiles" / "g.json",
          std::string(R"({"name":"g","root":")") + game.generic_u8string() + R"(","assets":[{"name":"Textures","path":"tex"}]})");
    assets::AssetsPlugin plugin(home.u8string(), "", nullptr);

    const auto roots = plugin.handle(get("/assets/api/roots"), "/api/roots");
    EXPECT_EQ(roots.status, 200);
    EXPECT_NE(roots.body.find("Textures"), std::string::npos);

    const auto list = plugin.handle(get("/assets/api/list", {{"root", "0"}, {"dir", ""}}), "/api/list");
    EXPECT_EQ(list.status, 200);
    EXPECT_NE(list.body.find("sky.png"), std::string::npos);

    const auto img = plugin.handle(get("/assets/api/file", {{"root", "0"}, {"path", "sky.png"}}), "/api/file");
    EXPECT_EQ(img.status, 200);
    EXPECT_EQ(img.body, "PNGDATA");
    EXPECT_EQ(img.headers.at("content-type"), "image/png");

    EXPECT_EQ(plugin.handle(get("/assets/api/file", {{"root", "0"}, {"path", "hero.glb"}}), "/api/file").status, 415);
    EXPECT_EQ(plugin.handle(get("/assets/api/file", {{"root", "0"}, {"path", "../profiles/g.json"}}), "/api/file").status, 404);
    EXPECT_EQ(plugin.handle(get("/assets/api/list", {{"root", "7"}}), "/api/list").status, 404);

    net::HttpRequest post = get("/assets/api/list");
    post.method = "POST";
    EXPECT_EQ(plugin.handle(post, "/api/list").status, 405);
}

// ---- web plugin --------------------------------------------------------------

TEST(WebPage, OriginAndCsp) {
    EXPECT_EQ(web::url_origin("https://Play.Example.org/app?x=1"), "https://play.example.org");
    EXPECT_EQ(web::url_origin("http://127.0.0.1:7676/"), "http://127.0.0.1:7676");
    EXPECT_EQ(web::url_origin("javascript:alert(1)"), "");
    EXPECT_EQ(web::url_origin("https://user@evil.example/"), "");
    EXPECT_NE(web::page_csp("https://play.example.org/x").find("frame-src https://play.example.org;"), std::string::npos);
    EXPECT_NE(web::page_csp("").find("frame-src 'none'"), std::string::npos);
}

TEST(WebPage, ServesThePageAndConfig) {
    web::WebPlugin plugin({"https://play.example.org/", {"studio.example.org"}});
    const auto page = plugin.handle(get("/web/"), "/");
    EXPECT_EQ(page.status, 200);
    EXPECT_NE(page.body.find("Ergo Studio"), std::string::npos);
    EXPECT_NE(page.headers.at("content-security-policy").find("frame-src https://play.example.org"), std::string::npos);
    EXPECT_EQ(plugin.handle(get("/web/app.js"), "/app.js").status, 200);
    const auto cfg = plugin.handle(get("/web/api/config"), "/api/config");
    EXPECT_EQ(cfg.status, 200);
    EXPECT_NE(cfg.body.find("https://play.example.org/"), std::string::npos);
    EXPECT_NE(cfg.body.find("studio.example.org"), std::string::npos);
    EXPECT_EQ(plugin.handle(get("/web/nope"), "/nope").status, 404);
}

TEST(WebPage, AnUnusablePlayUrlIsNotOffered) {
    web::WebPlugin plugin({"javascript:alert(1)", {}});
    const auto cfg = plugin.handle(get("/web/api/config"), "/api/config");
    EXPECT_EQ(cfg.body.find("javascript"), std::string::npos);
}
