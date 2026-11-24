// The launcher's model (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A / B, and
// spec/plan/2026-11-23-studio-gen5-intake.md, 1): recent_projects.json text and file, the
// launcher's rows and operations, project style detection, the /api/launcher/* routes and the
// remote dev server's start / stop / switch.
#include "ergo/studio/project/dev_server.h"
#include "ergo/studio/project/launcher.h"
#include "ergo/studio/project/launcher_routes.h"
#include "ergo/studio/project/project_catalog.h"
#include "ergo/studio/project/project_style.h"
#include "ergo/studio/project/recent_projects.h"

#include "ergo/common/json_min.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

using namespace ergo::studio;
using namespace ergo::studio::project;
namespace fs    = std::filesystem;
namespace jsonm = ergo::common::jsonm;

namespace {

fs::path fresh_dir(const std::string& tag) {
    static std::atomic<int> n{0};
    const fs::path dir = fs::temp_directory_path() / ("ergo-project-" + tag + "-" + std::to_string(++n));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

RecentProject entry(const std::string& source, const std::string& opened = "2026-11-23T00:00:00Z") {
    return {source, "n-" + source, "r-" + source, "cpp-other", "game", opened};
}

/// Profiles are any existing file; the name is the file's stem, the style "other".
LauncherDeps fake_deps(std::vector<ProjectRef> found = {}) {
    LauncherDeps d;
    d.probe = [](const fs::path& source, std::string& err) -> std::optional<ProjectRef> {
        if (source.extension() != ".json") {
            err = "not a profile";
            return std::nullopt;
        }
        return ProjectRef{source.generic_u8string(), source.stem().u8string(), source.parent_path().u8string(), "other",
                          "lesson"};
    };
    d.discover = [found] { return found; };
    d.now      = [] { return std::string("2026-11-23T10:00:00Z"); };
    return d;
}

std::string key(const fs::path& p) { return Launcher::normalize(p.u8string()); }

class FakeProbe final : public FileProbe {
public:
    std::set<std::string>              paths;
    std::map<std::string, std::string> files;
    bool exists(const std::string& rel) const override { return paths.count(rel) > 0 || files.count(rel) > 0; }
    std::optional<std::string> read(const std::string& rel) const override {
        const auto it = files.find(rel);
        return it == files.end() ? std::nullopt : std::optional<std::string>(it->second);
    }
};

class Next final : public net::IHttpHandler {
public:
    net::HttpResponse handle(const net::HttpRequest&) override {
        ++calls;
        return net::HttpResponse::json(200, "{\"next\":true}");
    }
    int calls = 0;
};

net::HttpRequest request(const std::string& method, const std::string& path, const std::string& body = {},
                         net::Gate gate = net::Gate::local) {
    net::HttpRequest r;
    r.method          = method;
    r.path            = path;
    r.target          = path;
    r.body            = body;
    r.gate            = gate;
    r.headers["host"] = gate == net::Gate::local ? "127.0.0.1:5170" : "devbox.tail1234.ts.net";
    return r;
}

std::string body_with_source(const fs::path& p) {
    jsonm::JsonValue v = jsonm::JsonValue::make_object();
    v.set("source", jsonm::JsonValue::make_string(p.u8string()));
    return jsonm::serialize(v);
}

} // namespace

// ---- recent_projects.json text -------------------------------------------------

TEST(RecentProjects, RoundTripsThroughText) {
    const std::vector<RecentProject> list{entry("C:/a.json"), entry("C:/b.json", "")};
    std::string err;
    const auto back = parse_recent_projects(serialize_recent_projects(list), err);
    ASSERT_TRUE(back.has_value());
    ASSERT_EQ(back->size(), 2u);
    EXPECT_EQ((*back)[0].source, "C:/a.json");
    EXPECT_EQ((*back)[0].name, "n-C:/a.json");
    EXPECT_EQ((*back)[0].root, "r-C:/a.json");
    EXPECT_EQ((*back)[0].style, "cpp-other");
    EXPECT_EQ((*back)[0].domain, "game");
    EXPECT_EQ((*back)[0].last_opened, "2026-11-23T00:00:00Z");
    EXPECT_EQ((*back)[1].last_opened, "");
}

TEST(RecentProjects, KeepsTheFileOrderAndMergesDuplicates) {
    std::string err;
    const auto list = parse_recent_projects(
        R"({"version":1,"projects":[{"source":"b","name":"new"},{"source":"a"},{"source":"b","name":"old"}]})", err);
    ASSERT_TRUE(list.has_value());
    ASSERT_EQ(list->size(), 2u);
    EXPECT_EQ((*list)[0].source, "b");
    EXPECT_EQ((*list)[0].name, "new");   // the first one wins
    EXPECT_EQ((*list)[1].source, "a");
}

TEST(RecentProjects, RememberPutsFirstReplacesAndCaps) {
    std::vector<RecentProject> list;
    for (int i = 0; i < 40; ++i) remember_project(list, entry("p" + std::to_string(i)));
    ASSERT_EQ(list.size(), kMaxRecentProjects);
    EXPECT_EQ(list.front().source, "p39");
    EXPECT_EQ(list.back().source, "p10");
    remember_project(list, entry("p20", "later"));
    EXPECT_EQ(list.size(), kMaxRecentProjects);
    EXPECT_EQ(list.front().source, "p20");
    EXPECT_EQ(list.front().last_opened, "later");
    EXPECT_EQ(std::count_if(list.begin(), list.end(), [](const RecentProject& r) { return r.source == "p20"; }), 1);
    EXPECT_TRUE(forget_project(list, "p20"));
    EXPECT_FALSE(forget_project(list, "p20"));
}

TEST(RecentProjects, ParsingCutsAtTheLimit) {
    std::vector<RecentProject> many;
    for (int i = 0; i < 35; ++i) many.push_back(entry("p" + std::to_string(i)));
    std::string err;
    const auto back = parse_recent_projects(serialize_recent_projects(many), err);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->size(), kMaxRecentProjects);
}

TEST(RecentProjects, RejectsWhatIsNotTheCache) {
    std::string err;
    EXPECT_FALSE(parse_recent_projects("not json", err).has_value());
    EXPECT_FALSE(parse_recent_projects(R"({"version":2,"projects":[]})", err).has_value());
    EXPECT_FALSE(parse_recent_projects(R"({"version":1,"projects":{}})", err).has_value());
    EXPECT_FALSE(parse_recent_projects(R"({"version":1,"projects":[{"name":"no source"}]})", err).has_value());
    EXPECT_FALSE(parse_recent_projects(R"({"version":1,"projects":[3]})", err).has_value());
}

// ---- the file -------------------------------------------------------------------

TEST(ProjectCatalog, AMissingFileIsEmptyAndSaveLoadRoundTrips) {
    const fs::path file = fresh_dir("catalog") / "recent_projects.json";
    ProjectCatalog c(file);
    std::string err;
    EXPECT_TRUE(c.load(err));
    EXPECT_TRUE(c.projects().empty());
    c.remember(entry("a"));
    c.remember(entry("b"));
    ASSERT_TRUE(c.save(err));
    EXPECT_FALSE(fs::exists(fs::path(file).concat(".tmp")));

    ProjectCatalog again(file);
    ASSERT_TRUE(again.load(err));
    ASSERT_EQ(again.projects().size(), 2u);
    EXPECT_EQ(again.projects()[0].source, "b");
}

TEST(ProjectCatalog, ABrokenFileIsEmptyAndNeverOverwritten) {
    const fs::path file = fresh_dir("broken") / "recent_projects.json";
    write(file, "{ this is not the cache");
    ProjectCatalog c(file);
    std::string err;
    EXPECT_FALSE(c.load(err));
    EXPECT_TRUE(c.broken());
    EXPECT_TRUE(c.projects().empty());
    EXPECT_FALSE(err.empty());
    c.remember(entry("a"));   // the run still works in memory
    EXPECT_EQ(c.projects().size(), 1u);
    EXPECT_FALSE(c.save(err));
    EXPECT_EQ(read(file), "{ this is not the cache");
}

TEST(ProjectCatalog, NoFileMeansMemoryOnly) {
    ProjectCatalog c{fs::path()};
    std::string err;
    EXPECT_TRUE(c.load(err));
    c.remember(entry("a"));
    EXPECT_TRUE(c.save(err));
}

// ---- the launcher ----------------------------------------------------------------

TEST(Launcher, NothingIsOpenOnStartAndRowsAreRecentThenFound) {
    const fs::path dir = fresh_dir("rows");
    write(dir / "a.json", "{}");
    write(dir / "b.json", "{}");
    write(dir / "c.json", "{}");
    const fs::path file = dir / "recent_projects.json";
    write(file, serialize_recent_projects({{key(dir / "b.json"), "b", "", "unity", "cglive", "2026-11-22T00:00:00Z"},
                                           {key(dir / "gone.json"), "gone", "", "", "", "2026-11-21T00:00:00Z"}}));
    OpenProject open;
    Launcher l(ProjectCatalog(file), open,
               fake_deps({{(dir / "a.json").u8string(), "a", "", "other", ""}, {(dir / "b.json").u8string(), "b", "", "", ""},
                          {(dir / "c.json").u8string(), "c", "", "cpp-ludiars", "industrial"}}));
    EXPECT_FALSE(open.current().has_value());
    const auto rows = l.rows();
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0].name, "b");
    EXPECT_TRUE(rows[0].recent);
    EXPECT_EQ(rows[0].style, "unity");
    EXPECT_EQ(rows[0].domain, "cglive");
    EXPECT_EQ(rows[1].name, "gone");
    EXPECT_TRUE(rows[1].missing);
    EXPECT_EQ(rows[2].name, "a");
    EXPECT_FALSE(rows[2].recent);
    EXPECT_EQ(rows[3].name, "c");
    EXPECT_EQ(rows[3].style, "cpp-ludiars");
    EXPECT_EQ(rows[3].domain, "industrial");
    EXPECT_TRUE(l.notice().empty());
}

TEST(Launcher, OpeningRecordsFirstAndBecomesTheOpenProject) {
    const fs::path dir = fresh_dir("open");
    write(dir / "a.json", "{}");
    write(dir / "b.json", "{}");
    const fs::path file = dir / "recent_projects.json";
    OpenProject open;
    Launcher l(ProjectCatalog(file), open, fake_deps());
    std::string err;
    const uint64_t before = l.revision();
    ASSERT_TRUE(l.open((dir / "a.json").u8string(), err));
    ASSERT_TRUE(l.open((dir / "b.json").u8string(), err));
    EXPECT_GT(l.revision(), before);
    ASSERT_TRUE(open.current().has_value());
    EXPECT_EQ(open.current()->name, "b");
    EXPECT_EQ(open.current()->style, "other");

    ProjectCatalog saved(file);
    ASSERT_TRUE(saved.load(err));
    ASSERT_EQ(saved.projects().size(), 2u);
    EXPECT_EQ(saved.projects()[0].source, key(dir / "b.json"));
    EXPECT_EQ(saved.projects()[0].last_opened, "2026-11-23T10:00:00Z");
    EXPECT_EQ(saved.projects()[0].style, "other");
    EXPECT_EQ(saved.projects()[0].domain, "lesson");

    const auto rows = l.rows();
    EXPECT_TRUE(rows[0].open);
    EXPECT_FALSE(rows[1].open);
    EXPECT_TRUE(l.close());
    EXPECT_FALSE(open.current().has_value());
    EXPECT_FALSE(l.close());
}

TEST(Launcher, AMissingOrUnreadableProfileDoesNotOpen) {
    const fs::path dir = fresh_dir("missing");
    write(dir / "notes.txt", "x");
    OpenProject open;
    Launcher l(ProjectCatalog(dir / "recent_projects.json"), open, fake_deps());
    std::string err;
    EXPECT_FALSE(l.open((dir / "nope.json").u8string(), err));
    EXPECT_FALSE(err.empty());
    EXPECT_FALSE(l.open((dir / "notes.txt").u8string(), err));
    EXPECT_FALSE(open.current().has_value());
    EXPECT_TRUE(l.rows().empty());
}

TEST(Launcher, AddListsWithoutOpeningAndRemoveClosesTheOpenOne) {
    const fs::path dir = fresh_dir("add");
    write(dir / "a.json", "{}");
    OpenProject open;
    Launcher l(ProjectCatalog(dir / "recent_projects.json"), open, fake_deps());
    std::string err;
    ASSERT_TRUE(l.add((dir / "a.json").u8string(), err));
    EXPECT_FALSE(open.current().has_value());
    ASSERT_EQ(l.rows().size(), 1u);
    EXPECT_EQ(l.rows()[0].last_opened, "");
    ASSERT_TRUE(l.open((dir / "a.json").u8string(), err));
    ASSERT_TRUE(l.remove((dir / "a.json").u8string(), err));
    EXPECT_FALSE(open.current().has_value());
    EXPECT_TRUE(l.rows().empty());
    EXPECT_FALSE(l.remove((dir / "a.json").u8string(), err));
}

TEST(Launcher, ABrokenCacheShowsANoticeAndStaysAsItIs) {
    const fs::path dir = fresh_dir("notice");
    write(dir / "a.json", "{}");
    write(dir / "recent_projects.json", "[]");
    OpenProject open;
    Launcher l(ProjectCatalog(dir / "recent_projects.json"), open, fake_deps());
    EXPECT_FALSE(l.notice().empty());
    std::string err;
    EXPECT_TRUE(l.open((dir / "a.json").u8string(), err));   // opening still works
    EXPECT_EQ(read(dir / "recent_projects.json"), "[]");
}

// ---- project style ---------------------------------------------------------------

TEST(ProjectStyle, UnityNeedsTheVersionFileAndAssets) {
    FakeProbe p;
    p.files["ProjectSettings/ProjectVersion.txt"] = "m_EditorVersion: 2022.3.10f1\nm_EditorVersionWithRevision: x\n";
    EXPECT_EQ(detect(p), ProjectStyle::other);   // no Assets/
    p.paths.insert("Assets");
    EXPECT_EQ(detect(p), ProjectStyle::unity);
    EXPECT_EQ(unity_editor_version(p), "2022.3.10f1");
}

TEST(ProjectStyle, CMakeThatPullsInLudiarsLibraries) {
    const std::vector<std::string> ludiars{
        "find_package(Pictor REQUIRED)\n",
        "add_subdirectory(external/ergo)\n",
        "FetchContent_Declare(lapilli GIT_REPOSITORY x)\n",
        "set(ERGO_PACKAGES pictor)\n",
    };
    for (const auto& text : ludiars) {
        FakeProbe p;
        p.files["CMakeLists.txt"] = "cmake_minimum_required(VERSION 3.20)\nproject(game)\n" + text;
        EXPECT_EQ(detect(p), ProjectStyle::cpp_ludiars);
    }
    FakeProbe sub;
    sub.files["CMakeLists.txt"] = "project(game)\n";
    sub.paths.insert("lib/lapilli");
    EXPECT_EQ(detect(sub), ProjectStyle::cpp_ludiars);
    FakeProbe modules;
    modules.files["CMakeLists.txt"] = "project(game)\n";
    modules.files[".gitmodules"]    = "[submodule \"lib/lapilli\"]\n\tpath = lib/lapilli\n";
    EXPECT_EQ(detect(modules), ProjectStyle::cpp_ludiars);
}

TEST(ProjectStyle, OtherCMakeAndEverythingElse) {
    FakeProbe cpp;
    cpp.files["CMakeLists.txt"] = "project(tool)\nfind_package(SDL2)\n# pictor is mentioned in a comment only\n";
    EXPECT_EQ(detect(cpp), ProjectStyle::cpp_other);
    FakeProbe web;
    web.files["package.json"] = "{}";
    EXPECT_EQ(detect(web), ProjectStyle::other);
    EXPECT_EQ(unity_editor_version(web), "");
}

TEST(ProjectStyle, NamesRoundTrip) {
    for (ProjectStyle s : {ProjectStyle::unity, ProjectStyle::cpp_ludiars, ProjectStyle::cpp_other, ProjectStyle::other}) {
        EXPECT_EQ(parse_style(style_name(s)), s);
    }
    EXPECT_FALSE(parse_style("unreal").has_value());
}

TEST(ProjectStyle, DiskProbeReadsARealFolder) {
    const fs::path dir = fresh_dir("disk");
    write(dir / "ProjectSettings" / "ProjectVersion.txt", "m_EditorVersion: 6000.0.1f1\r\n");
    fs::create_directories(dir / "Assets");
    const DiskProbe p(dir);
    EXPECT_EQ(detect(p), ProjectStyle::unity);
    EXPECT_EQ(unity_editor_version(p), "6000.0.1f1");
    EXPECT_FALSE(p.read("Assets").has_value());   // a folder is not a file
}

// ---- /api/launcher/* -----------------------------------------------------------

TEST(LauncherRoutes, ListsOpensAndClosesAndPassesTheRestOn) {
    const fs::path dir = fresh_dir("routes");
    write(dir / "a.json", "{}");
    OpenProject open;
    Launcher l(ProjectCatalog(dir / "recent_projects.json"), open, fake_deps({{(dir / "a.json").u8string(), "a", "", "other", "lesson"}}));
    Next next;
    LauncherRoutes routes(l, next);

    auto res = routes.handle(request("GET", "/api/launcher/projects"));
    EXPECT_EQ(res.status, 200);
    jsonm::JsonValue v;
    ASSERT_TRUE(jsonm::parse(res.body, v));
    EXPECT_TRUE(v.find("open")->is_null());
    ASSERT_EQ(v.find("projects")->a->size(), 1u);
    EXPECT_EQ((*v.find("projects")->a)[0].find("style")->as_string(), "other");
    EXPECT_EQ((*v.find("projects")->a)[0].find("domain")->as_string(), "lesson");

    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/open", body_with_source(dir / "a.json"))).status, 200);
    ASSERT_TRUE(jsonm::parse(routes.handle(request("GET", "/api/launcher/projects")).body, v));
    EXPECT_EQ(v.find("open")->find("name")->as_string(), "a");
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/close", "{}")).status, 200);
    EXPECT_FALSE(open.current().has_value());

    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/open", body_with_source(dir / "none.json"))).status, 409);
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/open", "nope")).status, 400);
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/open", "{}")).status, 400);
    EXPECT_EQ(routes.handle(request("GET", "/api/launcher/open")).status, 405);
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/projects")).status, 405);
    EXPECT_EQ(routes.handle(request("GET", "/api/launcher/other")).status, 404);
    EXPECT_EQ(next.calls, 0);
    EXPECT_EQ(routes.handle(request("GET", "/assets/api/roots")).status, 200);
    EXPECT_EQ(next.calls, 1);
}

TEST(LauncherRoutes, AddAndRemoveAreRefusedOnTheRemoteGate) {
    const fs::path dir = fresh_dir("guard");
    write(dir / "a.json", "{}");
    OpenProject open;
    Launcher l(ProjectCatalog(dir / "recent_projects.json"), open, fake_deps());
    Next next;
    LauncherRoutes routes(l, next);
    const std::string body = body_with_source(dir / "a.json");

    // Decided by the gate the request came in on, not by its Host.
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/add", body, net::Gate::remote)).status, 403);
    EXPECT_TRUE(l.rows().empty());
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/add", body)).status, 200);
    EXPECT_EQ(l.rows().size(), 1u);
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/remove", body, net::Gate::remote)).status, 403);
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/remove", body)).status, 200);
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/remove", body)).status, 404);
    // Opening is allowed from a remote page (it changes no list on disk beyond the recency).
    EXPECT_EQ(routes.handle(request("POST", "/api/launcher/open", body, net::Gate::remote)).status, 200);
}

// ---- the remote dev server (B) ------------------------------------------------------

namespace {

/// A server that only counts: how many are running, on which ports.
struct FakeServers {
    int  running = 0;
    int  started = 0;
    bool fail    = false;

    struct Serving final : IServing {
        explicit Serving(FakeServers& o) : owner(o) { ++owner.running; }
        ~Serving() override { --owner.running; }
        uint16_t                local_port() const override { return 5170; }
        std::optional<uint16_t> remote_port() const override { return remote; }
        FakeServers&            owner;
        std::optional<uint16_t> remote;
    };

    std::optional<uint16_t> remote;

    ServeFn serve() {
        return [this](std::string& err) -> std::unique_ptr<IServing> {
            if (fail) {
                err = "port taken";
                return nullptr;
            }
            ++started;
            auto s    = std::make_unique<Serving>(*this);
            s->remote = remote;
            return s;
        };
    }
};

struct DevFixture {
    DevFixture() : dir(fresh_dir("dev")) {
        write(dir / "a.json", "{}");
        write(dir / "b.json", "{}");
        launcher = std::make_unique<Launcher>(ProjectCatalog(dir / "recent_projects.json"), open, fake_deps());
    }
    std::string source(const char* name) const { return Launcher::normalize((dir / name).u8string()); }
    fs::path                  dir;
    OpenProject               open;
    std::unique_ptr<Launcher> launcher;
    FakeServers               servers;
};

} // namespace

TEST(DevServer, StartOpensTheProjectAndServesItStopClosesIt) {
    DevFixture f;
    DevServer dev(*f.launcher, f.servers.serve(), {"127.0.0.1", "/shell/", {"studio.example.com"}});
    EXPECT_FALSE(dev.status().running);
    std::string err;
    ASSERT_TRUE(dev.start(f.source("a.json"), err));
    EXPECT_EQ(f.servers.running, 1);
    ASSERT_TRUE(f.open.current().has_value());
    EXPECT_EQ(f.open.current()->source, f.source("a.json"));
    const DevServerStatus s = dev.status();
    EXPECT_TRUE(s.running);
    EXPECT_EQ(s.source, f.source("a.json"));
    EXPECT_EQ(s.local_url, "http://127.0.0.1:5170/shell/");
    EXPECT_TRUE(s.remote_target.empty());   // the remote gate is off by default
    EXPECT_TRUE(s.remote_urls.empty());

    const uint64_t before = dev.revision();
    dev.stop();
    EXPECT_GT(dev.revision(), before);
    EXPECT_EQ(f.servers.running, 0);
    EXPECT_FALSE(f.open.current().has_value());
    EXPECT_FALSE(dev.status().running);
}

TEST(DevServer, AnotherProjectStopsTheCurrentOneFirst) {
    DevFixture f;
    DevServer dev(*f.launcher, f.servers.serve());
    std::string err;
    ASSERT_TRUE(dev.start(f.source("a.json"), err));
    ASSERT_TRUE(dev.start(f.source("a.json"), err));   // already serving it: nothing restarts
    EXPECT_EQ(f.servers.started, 1);
    ASSERT_TRUE(dev.start(f.source("b.json"), err));
    EXPECT_EQ(f.servers.started, 2);
    EXPECT_EQ(f.servers.running, 1);   // one project at a time
    EXPECT_EQ(f.open.current()->source, f.source("b.json"));
    EXPECT_EQ(dev.status().source, f.source("b.json"));
}

TEST(DevServer, AFailedStartLeavesNothingOpen) {
    DevFixture f;
    f.servers.fail = true;
    DevServer dev(*f.launcher, f.servers.serve());
    std::string err;
    EXPECT_FALSE(dev.start(f.source("a.json"), err));
    EXPECT_EQ(err, "port taken");
    EXPECT_FALSE(f.open.current().has_value());
    EXPECT_FALSE(dev.status().running);

    f.servers.fail = false;
    err.clear();
    EXPECT_FALSE(dev.start(f.source("none.json"), err));   // no such profile: no server either
    EXPECT_EQ(f.servers.started, 0);
}

TEST(DevServer, TheRemoteGateShowsItsTargetAndTheNames) {
    DevFixture f;
    f.servers.remote = 5171;
    DevServer dev(*f.launcher, f.servers.serve(), {"127.0.0.1", "/shell/", {"studio.example.com", "devbox.tail1234.ts.net"}});
    std::string err;
    ASSERT_TRUE(dev.start(f.source("a.json"), err));
    const DevServerStatus s = dev.status();
    EXPECT_EQ(s.remote_target, "127.0.0.1:5171");
    ASSERT_EQ(s.remote_urls.size(), 2u);
    EXPECT_EQ(s.remote_urls[0], "https://studio.example.com/shell/");
    EXPECT_EQ(s.remote_urls[1], "https://devbox.tail1234.ts.net/shell/");
}

TEST(DevServer, ServesWithoutAProjectForAHeadlessStart) {
    DevFixture f;
    DevServer dev(*f.launcher, f.servers.serve());
    std::string err;
    ASSERT_TRUE(dev.serve_without_project(err));
    EXPECT_TRUE(dev.status().running);
    EXPECT_TRUE(dev.status().source.empty());
    EXPECT_FALSE(f.open.current().has_value());
    ASSERT_TRUE(dev.start(f.source("a.json"), err));   // the launcher's row: restarts on the project
    EXPECT_EQ(f.servers.running, 1);
    EXPECT_EQ(dev.status().source, f.source("a.json"));
}
