#include "fake_ws.h"
#include "plugins/common/file_io.h"
#include "plugins/common/safe_path.h"
#include "plugins/rive/rive_plugin.h"
#include "plugins/visus/visus_plugin.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace studio_test;
namespace fs = std::filesystem;

namespace {

class TempDir {
public:
    TempDir() {
        static std::atomic<int> n{0};
        path_ = fs::temp_directory_path() / ("ergo_studio_files_" + std::to_string(++n));
        fs::remove_all(path_);
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    const fs::path& path() const { return path_; }
    void write(const std::string& rel, const std::string& text) const {
        std::string err;
        ASSERT_TRUE(write_text_file(path_ / fs::u8path(rel), text, err));
    }

private:
    fs::path path_;
};

} // namespace

TEST(StudioSafePath, StaysInsideRoot) {
    TempDir d;
    const auto inside = resolve_within(d.path(), "a/b.json");
    ASSERT_TRUE(inside.has_value());
    EXPECT_EQ(relative_slash(d.path(), *inside), "a/b.json");
    EXPECT_TRUE(resolve_within(d.path(), "").has_value());        // the root itself
    EXPECT_TRUE(resolve_within(d.path(), "a/../b").has_value());
    EXPECT_FALSE(resolve_within(d.path(), "..").has_value());
    EXPECT_FALSE(resolve_within(d.path(), "../x").has_value());
    EXPECT_FALSE(resolve_within(d.path(), "a/../../x").has_value());
    EXPECT_FALSE(resolve_within(d.path(), d.path().parent_path().u8string()).has_value());
    EXPECT_TRUE(resolve_within(d.path(), (d.path() / "x").u8string()).has_value());   // absolute inside
#ifdef _WIN32
    const std::string other = d.path().root_name().u8string() == "Z:" ? "Y:\\x" : "Z:\\x";
    EXPECT_FALSE(resolve_within(d.path(), other).has_value());   // cross-drive escape
    EXPECT_FALSE(resolve_within(d.path(), "\\\\server\\share\\x").has_value());
#endif
}

TEST(StudioVisus, ScanLoadSave) {
    TempDir d;
    d.write("assets/visus/b.visus.json", R"({"name":"B","geometry":{"kind":"custom"},"materials":[1,2]})");
    d.write("assets/visus/a.visus.json", R"({"name":"A"})");
    d.write("assets/visus/broken.visus.json", "{nope");
    d.write("node_modules/x.visus.json", R"({"name":"skip"})");
    d.write(".hidden/y.visus.json", R"({"name":"skip"})");

    visus::VisusPlugin p({d.path()}, quiet());
    const auto files = p.store().scan(d.path());
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(files[0].rel_path, "assets/visus/a.visus.json");
    EXPECT_EQ(files[0].geometry_kind, "none");
    EXPECT_EQ(files[1].materials_count, 2u);
    EXPECT_EQ(files[1].mtime.size(), 24u);   // 2026-11-06T01:02:03.000Z

    EXPECT_TRUE(p.store().load(0, "assets/visus/a.visus.json").ok);
    EXPECT_EQ(p.store().load(1, "a").status, 400);
    EXPECT_EQ(p.store().load(0, "../x").err, "path outside root");
    EXPECT_EQ(p.store().load(0, "missing.visus.json").status, 404);

    Client c = open(p, 1);
    EXPECT_EQ(p.save(0, "new/c.visus.json", "{not json").status, 400);
    EXPECT_EQ(p.save(0, "c.json", "{}").status, 400);
    ASSERT_TRUE(p.save(0, "new/c.visus.json", R"({"name":"C"})").ok);
    EXPECT_EQ(field(c.conn->last("saved"), "rel_path")->as_string(), "new/c.visus.json");
    EXPECT_EQ(p.store().scan(d.path()).size(), 3u);

    net::HttpRequest r = req("GET");
    r.query = {{"root", "x"}};
    EXPECT_EQ(p.handle(r, "/api/scan").status, 400);
}

TEST(StudioVisus, NoRootsIsAnErrorNotTheWorkingDirectory) {
    auto p = visus::make_visus_plugin("", quiet());
    EXPECT_EQ(p->http()->handle(req("GET"), "/api/scan").status, 503);
    EXPECT_NE(field(p->health()->snapshot(), "warning"), nullptr);
    EXPECT_EQ(visus::parse_roots(" a ;; b ").size(), 2u);
}

TEST(StudioRive, PublishedMetaReachesEveryone) {
    rive::RivePlugin p;
    Client a = open(p, 1);
    EXPECT_TRUE(field(a.conn->last("meta"), "meta")->is_null());
    Client b = open(p, 2);
    EXPECT_EQ(field(a.conn->last("meta"), "clients")->as_number(), 2.0);
    b.h.on_text(R"({"op":"publish","meta":{"name":"hero.riv","size":10,"artboards":[]}})");
    EXPECT_EQ(field(*field(a.conn->last("meta"), "meta"), "name")->as_string(), "hero.riv");
    b.h.on_text(R"({"op":"publish","meta":"not an object"})");
    EXPECT_EQ(field(p.meta(), "name")->as_string(), "hero.riv");
    EXPECT_TRUE(field(p.snapshot(), "has_meta")->as_bool());
    b.h.on_close();
    EXPECT_EQ(field(a.conn->last("meta"), "clients")->as_number(), 1.0);
}
