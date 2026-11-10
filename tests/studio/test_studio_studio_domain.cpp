#include "fake_http.h"
#include "fake_ws.h"
#include "plugins/studio/actio_client.h"
#include "plugins/studio/concordia_client.h"
#include "plugins/studio/delegation_request.h"
#include "plugins/studio/endpoints.h"
#include "plugins/studio/mutation_guard.h"
#include "plugins/studio/plugin_source.h"
#include "plugins/studio/resource_sync.h"
#include "plugins/studio/source_sync.h"
#include "plugins/studio/studio_config.h"

#include <gtest/gtest.h>

#include <map>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::studio;
using namespace studio_test;

namespace {
EnvFn env_of(std::map<std::string, std::string> m) {
    return [m](const std::string& k) {
        const auto it = m.find(k);
        return it == m.end() ? std::string() : it->second;
    };
}
} // namespace

TEST(StudioConfig, ReadsEnvironmentWithDefaults) {
    const auto c = load_studio_config(env_of({{"EXCUBITOR_URL", "http://x:1/"}, {"ERGO_PLUGIN_OWNERS", " a, ,B "},
                                              {"ERGO_STUDIO_TIMEOUT_MS", "abc"}}),
                                      "C:/Users/u");
    EXPECT_EQ(c.excubitor_url.value_or(""), "http://x:1");
    ASSERT_EQ(c.plugin_owners.size(), 2u);
    EXPECT_EQ(c.plugin_owners[1], "B");
    EXPECT_EQ(c.request_timeout_ms, 8000);
    EXPECT_EQ(std::string(c.actio_mode()), "local");
    EXPECT_NE(c.home.find(".ergo-studio"), std::string::npos);
    const auto d = load_studio_config(env_of({{"ERGO_ACTIO_TOKEN", "t"}}), "");
    EXPECT_EQ(std::string(d.actio_mode()), "token");
    EXPECT_EQ(d.plugin_owners.at(0), "LUDIARS");
    EXPECT_TRUE(d.home.empty());   // no user home and no ERGO_STUDIO_HOME: not guessed
}

TEST(StudioEndpoints, CatalogPortAndTopology) {
    const std::string yaml =
        "services:\n  - code: concordia\n    port: 11111\n  -   code: excubitor   # self\n    ports:\n      - port: 1\n"
        "    port: 17332 # main\n  - code: other\n    port: 2\n";
    EXPECT_EQ(excubitor_port_from_catalog(yaml).value_or(0), 17332);
    EXPECT_FALSE(excubitor_port_from_catalog("- code: x\n    port: 3\n").has_value());

    StudioConfig cfg;
    cfg.curare_url = std::string("https://cu.remote");
    const auto s = pick_endpoints({{"ACTIO_URL", "http://127.0.0.1:17880/"}, {"CURARE_URL", "http://local"}, {"FOO_URL", "u"}},
                                  cfg, std::string("http://ex"));
    EXPECT_EQ(s.urls.at(Service::Actio).value_or(""), "http://127.0.0.1:17880");
    EXPECT_EQ(s.urls.at(Service::Curare).value_or(""), "https://cu.remote");   // explicit wins
    EXPECT_FALSE(s.urls.at(Service::Concordia).has_value());
    EXPECT_EQ(s.all.at("FOO"), "u");

    auto http = std::make_shared<FakeHttp>();
    http->routes["GET http://ex/api/v1/topology"] = {200, R"({"env":{"CONCORDIA_URL":"http://cc"}})"};
    ServiceCaller caller(http, 100);
    StudioConfig c2;
    c2.excubitor_url = std::string("http://ex");
    EndpointResolver r(c2, caller);
    EXPECT_EQ(r.url(Service::Concordia).value_or(""), "http://cc");
    EXPECT_EQ(r.snapshot().topology, "ok");
    http->routes.clear();
    EXPECT_EQ(r.snapshot(true).topology, "unavailable");
    EXPECT_FALSE(r.url(Service::Concordia).has_value());   // no stale endpoint after a failure
}

TEST(StudioServiceCall, ClassifiesAndSummarises) {
    auto http = std::make_shared<FakeHttp>();
    http->routes["GET http://s/a"] = {404, R"({"error":"no such task"})"};
    http->routes["GET http://s/b"] = {200, "not json"};
    http->routes["GET http://s/c"] = {204, ""};
    ServiceCaller c(http, 100);
    const auto a = c.call(std::string("http://s"), "/a");
    EXPECT_FALSE(a.ok);
    EXPECT_EQ(a.kind, FailureKind::NotFound);
    EXPECT_EQ(a.message, "no such task");
    EXPECT_EQ(c.call(std::string("http://s"), "/b").kind, FailureKind::BadResponse);
    EXPECT_TRUE(c.call(std::string("http://s"), "/c").ok);
    EXPECT_EQ(c.call(std::nullopt, "/x").kind, FailureKind::NotConfigured);
    EXPECT_EQ(c.call(std::string("http://s"), "/zzz").kind, FailureKind::Unreachable);
    EXPECT_EQ(summarize_error(std::string(400, 'x')).size(), 300u);
    EXPECT_EQ(summarize_error("a \n  b"), "a b");
    EXPECT_EQ(send_result("actio", a, JsonValue()).status, 404);
}

TEST(StudioClients, UnwrapAndAuth) {
    auto http = std::make_shared<FakeHttp>();
    http->routes["GET http://ex/api/v1/topology"] = {200, R"({"env":{"CONCORDIA_URL":"http://cc","ACTIO_URL":"http://ac"}})"};
    http->routes["GET http://cc/v1/delegation/templates"] = {200, R"({"templates":[{"call_name":"a","is_active":1},{"call_name":"b","is_active":0}]})"};
    http->routes["GET http://ac/api/tasks?scope=all&include=description&limit=200&project=Eg&status=open%2Cin_progress%2Cblocked"] =
        {200, R"({"tasks":[{"id":"1"}]})"};
    ServiceCaller caller(http, 100);
    StudioConfig cfg;
    cfg.excubitor_url = std::string("http://ex");
    EndpointResolver ep(cfg, caller);
    ConcordiaClient cc(ep, caller);
    const auto t = cc.templates();
    ASSERT_TRUE(t.ok);
    EXPECT_EQ(t.data.a->size(), 1u);
    ActioClient actio(ep, caller, std::string("secret"));
    const auto tasks = actio.tasks("Eg", false);
    ASSERT_TRUE(tasks.ok);
    EXPECT_EQ(tasks.data.a->size(), 1u);
    EXPECT_EQ(http->header(http->seen.size() - 1, "Authorization"), "Bearer secret");
}

TEST(StudioGuard, MutationsNeedLoopbackSameOriginAndJson) {
    EXPECT_FALSE(check_mutation("GET", std::nullopt, std::nullopt, std::nullopt).has_value());
    EXPECT_TRUE(check_mutation("POST", std::string("evil.com"), std::nullopt, std::string("application/json")).has_value());
    EXPECT_FALSE(check_mutation("POST", std::string("127.0.0.1:5170"), std::nullopt, std::string("application/json")).has_value());
    EXPECT_TRUE(check_mutation("POST", std::string("127.0.0.1:5170"), std::nullopt, std::string("text/plain")).has_value());
    EXPECT_FALSE(check_mutation("DELETE", std::string("localhost:5170"), std::nullopt, std::nullopt).has_value());
    EXPECT_TRUE(check_mutation("POST", std::string("127.0.0.1:5170"), std::string("http://evil.com"), std::string("application/json")).has_value());
    EXPECT_TRUE(check_mutation("POST", std::string("127.0.0.1:5170"), std::string("http://127.0.0.1:9999"), std::string("application/json")).has_value());
    EXPECT_FALSE(check_mutation("POST", std::string("[::1]:5170"), std::string("http://[::1]:5170"), std::string("application/json; charset=utf-8")).has_value());
    EXPECT_TRUE(check_mutation("POST", std::string("127.0.0.1:5170"), std::string("garbage"), std::string("application/json")).has_value());
}

TEST(StudioSourceSync, ParsesStatusAndDecides) {
    const auto s = parse_branch_status("# branch.oid abc\n# branch.head main\n# branch.upstream origin/main\n"
                                       "# branch.ab +0 -3\n1 .M N... 1 x\n? new.txt\n");
    EXPECT_EQ(s.branch.value_or(""), "main");
    EXPECT_EQ(s.behind, 3);
    EXPECT_EQ(s.changed, 1);
    EXPECT_EQ(s.untracked, 1);
    EXPECT_FALSE(decide_fast_forward(s).allowed);   // a change is present
    BranchStatus clean = s;
    clean.changed = 0;
    EXPECT_TRUE(decide_fast_forward(clean).allowed);
    clean.ahead = 1;
    EXPECT_NE(decide_fast_forward(clean).reason.find("手元だけ"), std::string::npos);
    EXPECT_FALSE(parse_branch_status("# branch.head (detached)\n").branch.has_value());
}

TEST(StudioResourceSync, ActionsNamesAndPaths) {
    EXPECT_EQ(decide_sync_action("h", true, true, std::string("h")), SyncAction::SkipSame);
    EXPECT_EQ(decide_sync_action("h", true, true, std::string("x")), SyncAction::Conflict);
    EXPECT_EQ(decide_sync_action("h", true, false, std::nullopt), SyncAction::Unavailable);
    EXPECT_EQ(decide_sync_action("h", false, false, std::nullopt), SyncAction::SkipSame);
    EXPECT_EQ(decide_sync_action("h", true, true, std::nullopt), SyncAction::Download);
    EXPECT_EQ(safe_file_name("a/b:c").value_or(""), "a_b_c");
    EXPECT_FALSE(safe_file_name("CON.txt").has_value());
    EXPECT_FALSE(safe_file_name("..").has_value());
    const std::filesystem::path repo = std::filesystem::temp_directory_path() / "repo";
    EXPECT_TRUE(resolve_inside(repo, "assets/tex").has_value());
    EXPECT_TRUE(resolve_inside(repo, "").has_value());
    EXPECT_FALSE(resolve_inside(repo, "../x").has_value());
    EXPECT_FALSE(resolve_inside(repo, "/abs").has_value());
    EXPECT_EQ(to_content_hash("ABCD"), "sha256:abcd");
}

TEST(StudioDelegation, BuildsInvokeWithinTheProject) {
    const JsonValue project = parse_json(R"({"code":"Eg","repo_path":"D:/LUDIARS/Ergo"})");
    const JsonValue tpl = parse_json(R"({"call_name":"impl","input_schema":[
        {"name":"task","required":true},{"name":"target_repo"},{"name":"note"}]})");
    InstructionInput in;
    in.call_name     = "impl";
    in.args          = parse_json(R"({"note":"  n  ","cwd":"C:/evil","extra":1})");
    in.task_text     = "Fix bug";
    in.actio_task_id = std::string("abc123");
    const auto r = build_invoke_request(project, tpl, in, "ergo-studio");
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(field(r.request, "cwd")->as_string(), "D:/LUDIARS/Ergo");   // never the editor's path
    const JsonValue& args = *field(r.request, "args");
    EXPECT_EQ(field(args, "task")->as_string(), "Fix bug");
    EXPECT_EQ(field(args, "target_repo")->as_string(), "D:/LUDIARS/Ergo");
    EXPECT_EQ(field(args, "note")->as_string(), "n");
    EXPECT_EQ(field(args, "cwd"), nullptr);                            // outside input_schema
    EXPECT_EQ(field(r.request, "taskflow_reference")->as_string(), "actio:abc123");
    EXPECT_EQ(field(r.request, "extra_prompt"), nullptr);

    InstructionInput missing;
    missing.call_name = "impl";
    EXPECT_FALSE(build_invoke_request(project, tpl, missing, "x").ok);
    in.actio_task_id = std::string("bad id");
    EXPECT_FALSE(build_invoke_request(project, tpl, in, "x").ok);
    in.call_name = "other";
    EXPECT_FALSE(build_invoke_request(project, tpl, in, "x").ok);

    EXPECT_TRUE(run_belongs_to(parse_json(R"({"cwd":"d:\\ludiars\\ergo\\"})"), "D:/LUDIARS/Ergo"));
    EXPECT_EQ(repo_slug_from_origin("git@github.com:LUDIARS/Ergo.git").value_or(""), "LUDIARS/Ergo");
    EXPECT_EQ(repo_slug_from_origin("https://github.com/LUDIARS/Ergo").value_or(""), "LUDIARS/Ergo");
    EXPECT_EQ(task_prompt("1", "T", ""), "T\n\n(Actio: actio:1)");
}

TEST(StudioPluginSource, ParsesAndChecksOwners) {
    const auto ok = parse_plugin_source("LUDIARS/AdventureCube:./tools/ac-web/plugins/@main", {"ludiars"});
    ASSERT_TRUE(ok.source.has_value());
    EXPECT_EQ(ok.source->subpath, "tools/ac-web/plugins");
    EXPECT_EQ(ok.source->rev, "main");
    EXPECT_EQ(pack_id(*ok.source), "LUDIARS/AdventureCube:tools/ac-web/plugins");
    const auto head = parse_plugin_source("LUDIARS/X:p", {"LUDIARS"});   // EXPECT_EQ binds references
    ASSERT_TRUE(head.source.has_value());
    EXPECT_EQ(head.source->rev, "HEAD");
    EXPECT_FALSE(parse_plugin_source("evil/X:p", {"LUDIARS"}).source.has_value());
    EXPECT_FALSE(parse_plugin_source("LUDIARS/X:../p", {"LUDIARS"}).source.has_value());
    EXPECT_FALSE(parse_plugin_source("LUDIARS/X:p@a..b", {"LUDIARS"}).source.has_value());
    EXPECT_FALSE(parse_plugin_source("nonsense", {"LUDIARS"}).source.has_value());
    EXPECT_TRUE(is_full_sha(std::string(40, 'a')));
    EXPECT_FALSE(is_full_sha("abc"));
}
