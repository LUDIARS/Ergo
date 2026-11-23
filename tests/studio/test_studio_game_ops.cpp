// Game workspace operations: workspace profiles, the services list and claim,
// tuning versions, and git (parsing, and a real repository in a temp folder).
#include "plugins/common/json_parse.h"
#include "plugins/common/json_util.h"
#include "plugins/common/workspace_profiles.h"
#include "plugins/git/git_ops.h"
#include "plugins/services/services_plugin.h"
#include "plugins/tuning/tuning_store.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>

using namespace ergo::studio::plugins;
namespace fs = std::filesystem;

namespace {

fs::path fresh_dir(const std::string& tag) {
    static std::atomic<int> n{0};
    const fs::path dir = fs::temp_directory_path() / ("ergo-game-ops-" + tag + "-" + std::to_string(++n));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

ProfileEnvFn env_with(const std::string& name, const std::string& value) {
    return [name, value](const std::string& key) -> std::optional<std::string> {
        if (key == name) return value;
        return std::nullopt;
    };
}

} // namespace

TEST(GameOpsProfiles, ReadsServicesTuningAndGitRelativeToTheRoot) {
    std::string err;
    const auto p = parse_workspace_profile(
        R"({"name":"amicii","root":"${LUDIARS_ROOT}/Amicii","services":["amicii-server"],
            "tuning":{"amicii-server":"games/september/balance/versions"},"git":["."]})",
        "C:/packs/amicii.studio.json", env_with("LUDIARS_ROOT", "D:/LUDIARS"), err);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->name, "amicii");
    EXPECT_EQ(p->root, fs::path("D:/LUDIARS/Amicii").lexically_normal());
    ASSERT_EQ(p->services.size(), 1u);
    EXPECT_EQ(p->tuning.at("amicii-server"), fs::path("D:/LUDIARS/Amicii/games/september/balance/versions").lexically_normal());
    ASSERT_EQ(p->git.size(), 1u);
    EXPECT_EQ(p->git[0], fs::path("D:/LUDIARS/Amicii").lexically_normal());
}

TEST(GameOpsProfiles, AnUnsetVariableMakesTheProfileUnusable) {
    std::string err;
    const auto p = parse_workspace_profile(R"({"root":"${NOPE}/x"})", "a.json", env_with("OTHER", "1"), err);
    EXPECT_FALSE(p.has_value());
    EXPECT_NE(err.find("NOPE"), std::string::npos);
}

TEST(GameOpsProfiles, LoadsFromHomeAndPluginFolders) {
    const fs::path home = fresh_dir("home"), packs = fresh_dir("packs");
    fs::create_directories(home / "profiles");
    std::ofstream(home / "profiles" / "a.json") << R"({"name":"a","services":["x"]})";
    std::ofstream(packs / "b.studio.json") << R"({"name":"b","git":["."]})";
    std::ofstream(packs / "ignored.json") << R"({"name":"ignored"})";
    std::ofstream(packs / "broken.studio.json") << "not json";
    const ProfileSet set = load_workspace_profiles(home, packs.u8string(), env_with("X", "1"));
    ASSERT_EQ(set.profiles.size(), 2u);
    EXPECT_EQ(set.profiles[0].name, "a");
    EXPECT_EQ(set.profiles[1].name, "b");
    EXPECT_EQ(set.errors.size(), 1u);   // the broken one is reported, not skipped silently
}

TEST(GameOpsServices, OnlyTheProfilesServicesAreOffered) {
    WorkspaceProfile p;
    p.name = "amicii";
    p.services = {"amicii-server"};
    const auto list = parse_json(
        R"({"services":[{"code":"amicii-server","name":"Amicii","state":"running","running":true,"port":4480},
                         {"code":"concordia","name":"Cc","state":"running","running":true}]})");
    ASSERT_TRUE(list.has_value());
    const JsonValue picked = services::pick_services(*list, {p});
    ASSERT_EQ(picked.a->size(), 1u);
    EXPECT_EQ((*picked.a)[0].find("code")->as_string(), "amicii-server");
    EXPECT_EQ((*picked.a)[0].find("profile")->as_string(), "amicii");
    // Without any profile listing services, everything is offered.
    EXPECT_EQ(services::pick_services(*list, {}).a->size(), 2u);
}

TEST(GameOpsServices, TheClaimNamesTheStudioAndTheService) {
    const JsonValue body = services::claim_body("ergo-studio", "amicii-server", "start");
    EXPECT_EQ(body.find("session_id")->as_string(), "ergo-studio");
    EXPECT_EQ(body.find("service")->as_string(), "amicii-server");
    EXPECT_FALSE(body.find("note")->as_string().empty());
}

TEST(GameOpsTuning, SavesListsLoadsAndNeverOverwrites) {
    const fs::path dir = fresh_dir("tuning");
    const tuning::TuningStore store(dir);
    const fs::path folder = store.folder("amicii-server", {});
    const auto values = parse_json(R"({"balance.a":1,"balance.b":2})");
    std::string err;
    const auto first = store.save(folder, "amicii-server", "やや速い", "note", *values, "2026-11-11T08:00:00Z", err);
    ASSERT_TRUE(first.has_value());
    const auto second = store.save(folder, "amicii-server", "やや速い", "", *values, "2026-11-11T08:00:00Z", err);
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(*first, *second);   // same name and second: a new file, not a rewrite
    const auto list = store.list(folder);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].values, 2u);
    const auto loaded = store.load(folder, *first, err);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->find("name")->as_string(), "やや速い");
    EXPECT_FALSE(store.load(folder, "../escape", err).has_value());
    EXPECT_FALSE(store.save(folder, "amicii-server", "", "", *values, "2026-11-11T08:00:00Z", err).has_value());
}

TEST(GameOpsTuning, TheDiffListsChangedAndNewValues) {
    const auto before = parse_json(R"({"a":1,"b":2})");
    const auto after = parse_json(R"({"a":1,"b":3,"c":4})");
    const JsonValue diff = tuning::diff_values(*before, *after);
    ASSERT_EQ(diff.a->size(), 2u);
    EXPECT_EQ((*diff.a)[0].find("name")->as_string(), "b");
    EXPECT_EQ((*diff.a)[1].find("name")->as_string(), "c");
    EXPECT_EQ(tuning::slug_of("Exhibit spots / v2"), "Exhibit-spots-v2");
}

TEST(GameOpsGit, ParsesPorcelainV2) {
    const JsonValue st = git::parse_status(
        "# branch.oid abc\n# branch.head feat/x\n# branch.upstream origin/feat/x\n# branch.ab +2 -1\n"
        "1 .M N... 100644 100644 100644 aaa bbb src/a.cpp\n? notes.txt\n");
    EXPECT_EQ(st.find("branch")->as_string(), "feat/x");
    EXPECT_EQ(st.find("ahead")->as_number(), 2);
    EXPECT_EQ(st.find("behind")->as_number(), 1);
    ASSERT_EQ(st.find("files")->a->size(), 2u);
    EXPECT_EQ((*st.find("files")->a)[0].find("path")->as_string(), "src/a.cpp");
    EXPECT_EQ((*st.find("files")->a)[1].find("state")->as_string(), "untracked");
    EXPECT_TRUE(git::valid_branch("feat/studio-game-ops"));
    EXPECT_FALSE(git::valid_branch("-f"));
    EXPECT_FALSE(git::valid_branch("a..b"));
    EXPECT_FALSE(git::valid_branch("has space"));
}

TEST(GameOpsGit, CommitsBranchesAndReportsOnARealRepository) {
    const auto runner = ergo::studio::platform::make_system_process_runner();
    if (!ergo::studio::platform::find_on_path("git")) return;   // no git here: nothing to check against
    const fs::path repo = fresh_dir("repo");
    // This machine's global hooks may refuse ref updates; the test repository uses none.
    const fs::path no_hooks = fresh_dir("hooks");
    const auto run = [&](std::vector<std::string> args) {
        ergo::studio::platform::ProcessSpec spec;
        spec.program = "git";
        spec.args = {"-c", "core.hooksPath=" + no_hooks.generic_u8string(), "-C", repo.u8string()};
        spec.args.insert(spec.args.end(), args.begin(), args.end());
        return runner->run(spec);
    };
    ASSERT_EQ(run({"init", "-b", "main"}).exit_code, 0);
    ASSERT_EQ(run({"config", "user.email", "studio@example.invalid"}).exit_code, 0);
    ASSERT_EQ(run({"config", "user.name", "Studio Test"}).exit_code, 0);
    ASSERT_EQ(run({"config", "core.hooksPath", no_hooks.generic_u8string()}).exit_code, 0);
    std::ofstream(repo / "a.txt") << "one";
    const git::GitOps ops(runner, 20000);
    std::string err;
    const auto st = ops.status(repo, err);
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->find("files")->a->size(), 1u);
    const auto committed = ops.commit_all(repo, "最初のコミット\n\n本文");
    ASSERT_TRUE(committed.ok);
    EXPECT_FALSE(ops.commit_all(repo, "   ").ok);   // no message, no commit
    const auto switched = ops.switch_to(repo, "feat/try", true);
    ASSERT_TRUE(switched.ok);
    const auto branches = ops.branches(repo, err);
    ASSERT_TRUE(branches.has_value());
    EXPECT_EQ(branches->a->size(), 2u);
    const auto log = ops.log(repo, 5, err);
    ASSERT_TRUE(log.has_value());
    ASSERT_EQ(log->a->size(), 1u);
    EXPECT_EQ((*log->a)[0].find("subject")->as_string(), "最初のコミット");
    EXPECT_FALSE(ops.switch_to(repo, "--orphan", true).ok);
}
