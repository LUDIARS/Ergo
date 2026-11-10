#include "fake_http.h"
#include "fake_ws.h"
#include "plugins/studio/connection_map.h"
#include "plugins/studio/plugin_ledger.h"
#include "plugins/studio/studio_plugin.h"

#include "ergo/studio/platform/sha256.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <map>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::studio;
using namespace studio_test;
namespace fs = std::filesystem;

namespace {

/// Answers by program + first args; records every spec.
class FakeRunner final : public platform::IProcessRunner {
public:
    std::map<std::string, platform::ProcessResult> answers;   ///< key: "git status" / "git fetch" / ...
    std::vector<platform::ProcessSpec>             seen;
    platform::ProcessResult run(const platform::ProcessSpec& s) override {
        seen.push_back(s);
        std::string key = s.program;
        for (const auto& a : s.args) {
            if (a == "-C" || a.find('/') != std::string::npos || a.find('\\') != std::string::npos) continue;
            key += " " + a;
            break;
        }
        const auto it = answers.find(key);
        if (it != answers.end()) return it->second;
        platform::ProcessResult r;
        r.started   = true;
        r.exit_code = 1;
        r.err       = "unexpected: " + key;
        return r;
    }
};

platform::ProcessResult ok_out(const std::string& out) {
    platform::ProcessResult r;
    r.started   = true;
    r.exit_code = 0;
    r.out       = out;
    return r;
}

struct Fixture {
    std::shared_ptr<FakeHttp>   http   = std::make_shared<FakeHttp>();
    std::shared_ptr<FakeRunner> runner = std::make_shared<FakeRunner>();
    std::unique_ptr<StudioPlugin> plugin;

    explicit Fixture(const fs::path& home = {}) {
        StudioConfig cfg;
        cfg.excubitor_url      = std::string("http://ex");
        cfg.plugin_owners      = {"LUDIARS"};
        cfg.request_timeout_ms = 100;
        cfg.home               = home.u8string();
        http->routes["GET http://ex/api/v1/topology"] = {200, R"({"env":{"CONCORDIA_URL":"http://cc","ACTIO_URL":"http://ac"}})"};
        http->routes["GET http://cc/v1/project-codes/admin"] =
            {200, R"({"entries":[{"code":"Eg","project":"Ergo","repo_path":"D:/LUDIARS/Ergo","repo_origin":"git@github.com:LUDIARS/Ergo.git"}]})"};
        http->routes["GET http://cc/v1/delegation/templates"] =
            {200, R"({"templates":[{"call_name":"impl","is_active":1,"input_schema":[{"name":"task","required":true},{"name":"target_repo"}]}]})"};
        plugin = std::make_unique<StudioPlugin>(StudioDeps::create(cfg, http, runner, {}), quiet());
    }

    net::HttpResponse call(const std::string& method, const std::string& path, const std::string& body = {}) {
        net::HttpRequest r = req(method, body);
        const auto q = path.find('?');
        r.path = path.substr(0, q);
        if (q != std::string::npos) {
            const std::string qs = path.substr(q + 1);
            const auto eq = qs.find('=');
            r.query[qs.substr(0, eq)] = qs.substr(eq + 1);
        }
        r.headers["host"] = "127.0.0.1:5170";
        if (method != "GET") r.headers["content-type"] = "application/json";
        return plugin->handle(r, r.path);
    }
};

} // namespace

TEST(StudioApi, MutationsAreGuarded) {
    Fixture f;
    net::HttpRequest r = req("POST", "{}");
    r.path = "/api/tasks";
    r.headers["host"]         = "evil.example";
    r.headers["content-type"] = "application/json";
    EXPECT_EQ(f.plugin->handle(r, r.path).status, 403);
    r.headers["host"]         = "127.0.0.1:5170";
    r.headers["content-type"] = "text/plain";
    EXPECT_EQ(f.plugin->handle(r, r.path).status, 403);
    EXPECT_EQ(f.call("GET", "/api/health").status, 200);
}

TEST(StudioApi, InstructUsesTheRegisteredRepoPath) {
    Fixture f;
    f.http->routes["POST http://cc/v1/delegation/invoke"] = {200, R"({"run_id":"r1"})"};
    const auto res = f.call("POST", "/api/projects/Eg/instruct", R"({"call_name":"impl","args":{"task":"do it","cwd":"C:/evil"}})");
    ASSERT_EQ(res.status, 201);
    const auto& sent = f.http->seen.back();
    const JsonValue body = parse_json(sent.body);
    EXPECT_EQ(field(body, "cwd")->as_string(), "D:/LUDIARS/Ergo");
    EXPECT_EQ(field(*field(body, "args"), "target_repo")->as_string(), "D:/LUDIARS/Ergo");
    EXPECT_EQ(field(*field(body, "args"), "cwd"), nullptr);
    EXPECT_EQ(f.call("POST", "/api/projects/Zz/instruct", R"({"call_name":"impl"})").status, 404);
    EXPECT_EQ(f.call("POST", "/api/projects/Eg/instruct", R"({"call_name":"nope"})").status, 404);
}

TEST(StudioApi, TaskValidationAndImplement) {
    Fixture f;
    EXPECT_EQ(f.call("POST", "/api/tasks", R"({"title":"x","status":"weird"})").status, 400);
    EXPECT_EQ(f.call("POST", "/api/tasks", R"({"description":"no title"})").status, 400);
    f.http->routes["GET http://ac/api/tasks/t1"] = {200, R"({"task":{"id":"t1","title":"Fix","description":"body"}})"};
    f.http->routes["POST http://cc/v1/delegation/invoke"] = {200, R"({"run_id":"r2"})"};
    ASSERT_EQ(f.call("POST", "/api/tasks/t1/implement", R"({"code":"Eg","call_name":"impl"})").status, 201);
    const JsonValue body = parse_json(f.http->seen.back().body);
    EXPECT_EQ(field(body, "taskflow_reference")->as_string(), "actio:t1");
    EXPECT_NE(field(*field(body, "args"), "task")->as_string().find("Fix\n\nbody"), std::string::npos);
}

TEST(StudioApi, FastForwardOnlyOnACleanMain) {
    Fixture f;
    f.runner->answers["git status"] = ok_out("# branch.head main\n# branch.upstream origin/main\n# branch.ab +0 -2\n1 .M x\n");
    f.runner->answers["git log"]    = ok_out("abc\0subj\0date");
    EXPECT_EQ(f.call("POST", "/api/sources/Eg/fast-forward", "{}").status, 409);   // a change is present
    f.runner->answers["git status"] = ok_out("# branch.head main\n# branch.upstream origin/main\n# branch.ab +0 -2\n");
    f.runner->answers["git merge"]  = ok_out("");
    EXPECT_EQ(f.call("POST", "/api/sources/Eg/fast-forward", "{}").status, 200);
    EXPECT_EQ(f.runner->seen.back().args.back(), "origin/main");
    const JsonValue detail = parse_json(f.call("GET", "/api/sources/Eg").body);
    EXPECT_EQ(field(detail, "repository")->as_string(), "LUDIARS/Ergo");
    EXPECT_TRUE(field(*field(detail, "fastForward"), "allowed")->as_bool());
}

TEST(StudioApi, PluginSourcesAreAllowListed) {
    Fixture f;
    EXPECT_EQ(f.call("POST", "/api/plugins/preview", R"({"source":"evil/x:p"})").status, 400);
    EXPECT_EQ(f.call("POST", "/api/plugins/install", R"({"token":"nope"})").status, 409);
}

TEST(StudioLedger, RoundTripAndPluginDirs) {
    const fs::path home = fs::temp_directory_path() / "ergo_studio_ledger";
    fs::remove_all(home);
    Ledger l;
    l.packs.push_back({"LUDIARS/X:p", "LUDIARS", "X", "p", std::string(40, 'a'), (home / "co").u8string(), true, "t", {"a", "b"}});
    l.packs.push_back({"LUDIARS/Y:q", "LUDIARS", "Y", "q", std::string(40, 'b'), (home / "co2").u8string(), false, "t", {}});
    std::string err;
    ASSERT_TRUE(write_ledger(home, l, err));
    const Ledger back = read_ledger(home);
    ASSERT_EQ(back.packs.size(), 2u);
    EXPECT_EQ(back.packs[0].plugins.size(), 2u);
    EXPECT_FALSE(back.packs[1].enabled);
    const std::string dirs = plugin_dirs_with_ledger("C:/game/packs", back);
    EXPECT_NE(dirs.find("C:/game/packs"), std::string::npos);
    EXPECT_NE(dirs.find("co"), std::string::npos);
    EXPECT_EQ(dirs.find("co2"), std::string::npos);   // disabled
    EXPECT_TRUE(parse_ledger("{broken").packs.empty());
    fs::remove_all(home);
}

TEST(StudioConnections, MapStatesAndEdges) {
    ProbeResult ok;
    ok.key = Service::Concordia;
    ok.ok  = true;
    ProbeResult denied;
    denied.key     = Service::Actio;
    denied.failure = FailureKind::Unauthorized;
    const JsonValue m = build_connection_map({ok, denied});
    const auto& nodes = *field(m, "nodes")->a;
    ASSERT_EQ(nodes.size(), 8u);
    std::map<std::string, std::string> state;
    for (const auto& n : nodes) state[field(n, "id")->as_string()] = field(n, "state")->as_string();
    EXPECT_EQ(state["concordia"], "ok");
    EXPECT_EQ(state["actio"], "unauthorized");
    EXPECT_EQ(state["curare"], "not_configured");
    EXPECT_EQ(platform::sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
