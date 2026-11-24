// IProfileSource (spec/plan/2026-11-23-studio-launcher-and-devserver.md): the game-ops plugins see
// only the project open in the launcher, nothing while none is open; the launcher's profile reading
// (an explicit "style" wins over detection).
#include "plugins/assets/assets_plugin.h"
#include "plugins/common/launcher_profiles.h"
#include "plugins/common/profile_source.h"
#include "plugins/play/play_plugin.h"
#include "plugins/tuning/tuning_plugin.h"

#include "ergo/studio/project/dev_server.h"
#include "ergo/studio/project/launcher.h"
#include "ergo/studio/project/open_project.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
namespace fs = std::filesystem;

namespace {

fs::path fresh_dir(const std::string& tag) {
    static std::atomic<int> n{0};
    const fs::path dir = fs::temp_directory_path() / ("ergo-profile-source-" + tag + "-" + std::to_string(++n));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

std::optional<std::string> no_env(const std::string&) { return std::nullopt; }

net::HttpRequest get(const std::string& path) {
    net::HttpRequest r;
    r.method = "GET";
    r.path   = path;
    r.target = path;
    return r;
}

/// Two games on disk, each with assets, tuning and a play target.
struct TwoGames {
    TwoGames() {
        home = fresh_dir("home");
        for (const char* name : {"alpha", "beta"}) {
            const fs::path root = home / name;
            fs::create_directories(root / "art");
            write(home / "profiles" / (std::string(name) + ".json"),
                  std::string(R"({"name":")") + name + R"(","root":")" + root.generic_u8string() +
                      R"(","assets":[{"name":"Art","path":"art"}],"tuning":{")" + name + R"(-server":"tune"},)" +
                      R"("play":[{"name":")" + name + R"(","run":{"cmd":"game"},"window":"W"}]})");
        }
    }
    project::ProjectRef ref(const char* name) const {
        return {(home / "profiles" / (std::string(name) + ".json")).u8string(), name, (home / name).u8string(), "other", ""};
    }
    fs::path home;
};

} // namespace

TEST(ProfileSource, EmptyWhileNoProjectIsOpenThenOnlyTheOpenOne) {
    TwoGames games;
    project::OpenProject open;
    const OpenProjectProfiles source(open, &no_env);
    EXPECT_TRUE(source.profiles().profiles.empty());
    EXPECT_TRUE(source.profiles().errors.empty());

    open.open(games.ref("beta"));
    const ProfileSet set = source.profiles();
    ASSERT_EQ(set.profiles.size(), 1u);
    EXPECT_EQ(set.profiles[0].name, "beta");

    open.close();
    EXPECT_TRUE(source.profiles().profiles.empty());
}

TEST(ProfileSource, AnUnreadableOpenProfileIsAnError) {
    const fs::path dir = fresh_dir("bad");
    write(dir / "bad.json", "{ nope");
    project::OpenProject open;
    open.open({(dir / "bad.json").u8string(), "bad", "", "", ""});
    const ProfileSet set = OpenProjectProfiles(open, &no_env).profiles();
    EXPECT_TRUE(set.profiles.empty());
    ASSERT_EQ(set.errors.size(), 1u);
}

TEST(ProfileSource, AssetsSeeOnlyTheOpenProject) {
    TwoGames games;
    project::OpenProject open;
    assets::AssetsPlugin plugin(std::make_shared<OpenProjectProfiles>(open, &no_env), nullptr);
    EXPECT_EQ(plugin.handle(get("/assets/api/roots"), "/api/roots").body, "[]");

    open.open(games.ref("alpha"));
    const std::string roots = plugin.handle(get("/assets/api/roots"), "/api/roots").body;
    EXPECT_NE(roots.find("\"alpha\""), std::string::npos);
    EXPECT_EQ(roots.find("\"beta\""), std::string::npos);
}

TEST(ProfileSource, TuningAndPlaySeeOnlyTheOpenProject) {
    TwoGames games;
    project::OpenProject open;
    open.open(games.ref("beta"));
    const auto source = std::make_shared<OpenProjectProfiles>(open, &no_env);

    tuning::TuningPlugin tuning(tuning::TuningStore(games.home / "tuning"), games.home.u8string(), source, nullptr);
    const std::string health = tuning.handle(get("/tuning/api/health"), "/api/health").body;
    EXPECT_NE(health.find("beta-server"), std::string::npos);
    EXPECT_EQ(health.find("alpha-server"), std::string::npos);

    std::vector<std::string> errors;
    const auto targets = play::profile_targets(source)(errors);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].name, "beta");
    EXPECT_TRUE(errors.empty());
}

// ergo_studio --project <profile.json>: the dev server serves that profile, and the plugins see it alone.
TEST(ProfileSource, TheServedProjectIsTheOnlyProfileThePluginsSee) {
    TwoGames games;
    project::OpenProject open;
    project::Launcher    launcher(project::ProjectCatalog(games.home / "recent_projects.json"), open,
                                  launcher_deps(games.home, "", &no_env));
    struct Serving final : project::IServing {
        uint16_t                local_port() const override { return 5170; }
        std::optional<uint16_t> remote_port() const override { return std::nullopt; }
    };
    project::DevServer dev(launcher, [](std::string&) { return std::make_unique<Serving>(); });
    const auto         source = std::make_shared<OpenProjectProfiles>(open, &no_env);
    EXPECT_TRUE(source->profiles().profiles.empty());   // the launcher: nothing open yet

    std::string err;
    ASSERT_TRUE(dev.start((games.home / "profiles" / "alpha.json").u8string(), err));
    ProfileSet set = source->profiles();
    ASSERT_EQ(set.profiles.size(), 1u);
    EXPECT_EQ(set.profiles[0].name, "alpha");
    assets::AssetsPlugin plugin(source, nullptr);
    const std::string roots = plugin.handle(get("/assets/api/roots"), "/api/roots").body;
    EXPECT_NE(roots.find("\"alpha\""), std::string::npos);
    EXPECT_EQ(roots.find("\"beta\""), std::string::npos);

    dev.stop();
    EXPECT_TRUE(source->profiles().profiles.empty());
}

TEST(ProfileSource, AFixedSetIsGivenAsIs) {
    ProfileSet set;
    set.errors.push_back("x: broken");
    const FixedProfiles fixed(set);
    EXPECT_EQ(fixed.profiles().errors.size(), 1u);
}

TEST(LauncherProfiles, AnExplicitStyleWinsAndOtherwiseTheRootIsDetected) {
    const fs::path dir = fresh_dir("style");
    write(dir / "unity" / "ProjectSettings" / "ProjectVersion.txt", "m_EditorVersion: 2022.3.1f1\n");
    fs::create_directories(dir / "unity" / "Assets");
    write(dir / "home" / "profiles" / "u.json", R"({"name":"u","root":")" + (dir / "unity").generic_u8string() + R"("})");
    write(dir / "home" / "profiles" / "x.json",
          R"({"name":"x","root":")" + (dir / "unity").generic_u8string() + R"(","style":"cpp-ludiars","domain":"cglive"})");
    const auto deps = launcher_deps(dir / "home", "", &no_env);

    std::string err;
    const auto u = deps.probe(dir / "home" / "profiles" / "u.json", err);
    ASSERT_TRUE(u.has_value());
    EXPECT_EQ(u->style, "unity");
    const auto x = deps.probe(dir / "home" / "profiles" / "x.json", err);
    ASSERT_TRUE(x.has_value());
    EXPECT_EQ(x->style, "cpp-ludiars");
    EXPECT_EQ(x->domain, "cglive");
    EXPECT_EQ(u->domain, "");
    EXPECT_FALSE(deps.probe(dir / "home" / "profiles" / "none.json", err).has_value());

    const auto found = deps.discover();
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].name, "u");
    EXPECT_FALSE(deps.now().empty());
}

TEST(LauncherProfiles, TheDomainIsAnyTextAndNotInterpreted) {
    std::string err;
    const auto any = parse_workspace_profile(R"({"name":"g","domain":"robot-arm-cell"})", "/x/g.json", &no_env, err);
    ASSERT_TRUE(any.has_value());
    EXPECT_EQ(any->domain, "robot-arm-cell");
    EXPECT_FALSE(parse_workspace_profile(R"({"name":"g","domain":3})", "/x/g.json", &no_env, err).has_value());
}

TEST(LauncherProfiles, AnUnknownStyleMakesTheProfileUnusable) {
    std::string err;
    EXPECT_FALSE(parse_workspace_profile(R"({"name":"g","style":"unreal"})", "/x/g.json", &no_env, err).has_value());
    EXPECT_NE(err.find("style"), std::string::npos);
    const auto ok = parse_workspace_profile(R"({"name":"g","style":"unity"})", "/x/g.json", &no_env, err);
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok->style, "unity");
}
