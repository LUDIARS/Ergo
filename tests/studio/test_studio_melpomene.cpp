#include "fake_http.h"
#include "fake_ws.h"
#include "plugins/common/file_io.h"
#include "plugins/musa_melpomene/melpomene_plugin.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <map>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::musa_melpomene;
using namespace studio_test;
namespace fs = std::filesystem;

namespace {

class FakeCreds final : public platform::ICredentialStore {
public:
    std::map<std::string, std::string> items;
    std::optional<std::string> get(const std::string& s, const std::string& a, std::string&) override {
        const auto it = items.find(a + "." + s);
        if (it == items.end()) return std::nullopt;
        return it->second;
    }
    bool set(const std::string& s, const std::string& a, const std::string& v, std::string&) override {
        items[a + "." + s] = v;
        return true;
    }
    bool remove(const std::string& s, const std::string& a, std::string&) override {
        items.erase(a + "." + s);
        return true;
    }
};

fs::path temp_settings() {
    static std::atomic<int> n{0};
    const fs::path dir = fs::temp_directory_path() / ("ergo_studio_melp_" + std::to_string(++n));
    fs::remove_all(dir);
    return dir / "settings.json";
}

const char* const kBase = "https://api.github.com/repos/acme/game";

const char* const kIssues = R"([
  {"number":1,"title":"Crash","state":"open","body":"boom\n\n---\n**Scene:** Forest\n**Object Path:** /root/Tree\n**World Position:** (1.00, 2.00, 3.00)\n**Priority:** High\n**Category:** Feature\n",
   "user":{"login":"kaz"},"labels":[{"name":"bug"}],"assignees":[{"login":"a"}],"milestone":{"number":4}},
  {"number":2,"title":"PR, not an issue","pull_request":{}},
  {"number":3,"title":"Closed","state":"closed","body":null}
])";

} // namespace

TEST(StudioMelpomeneModels, TicketRoundTripsMetadataThroughTheBody) {
    JsonValue arr;
    ASSERT_TRUE(ergo::common::jsonm::parse(kIssues, arr));
    const auto t = ticket_from_github(arr.a->at(0));
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->id, "github-1");
    EXPECT_EQ(t->scene_name.value_or(""), "Forest");
    EXPECT_EQ(t->object_path.value_or(""), "/root/Tree");
    ASSERT_TRUE(t->world_position.has_value());
    EXPECT_EQ((*t->world_position)[2], 3.0);
    EXPECT_EQ(t->priority, Priority::High);
    EXPECT_EQ(t->category, Category::Feature);
    EXPECT_EQ(t->milestone_id.value_or(0), 4u);
    EXPECT_EQ(t->author.value_or(""), "kaz");

    const std::string body = t->to_github_body();
    EXPECT_NE(body.find("**World Position:** (1.00, 2.00, 3.00)"), std::string::npos);
    EXPECT_NE(body.find("**Priority:** High"), std::string::npos);
    EXPECT_FALSE(ticket_from_github(jobj({{"title", jstr("no number")}})).has_value());

    Milestone m;
    m.open_issues   = 1;
    m.closed_issues = 3;
    EXPECT_EQ(m.progress_percent(), 75.0);
}

TEST(StudioMelpomeneConfig, TokenGoesToTheStoreNotTheFile) {
    const fs::path f = temp_settings();
    auto creds = std::make_shared<FakeCreds>();
    ConfigStore store(f, creds);
    MelpomeneConfig c;
    c.repository_owner = "acme";
    c.repository_name  = "game";
    c.github_token     = "ghp_secret";
    std::string err;
    ASSERT_TRUE(store.save(c, err));
    std::string text;
    {
        auto t = read_text_file(f, err);
        ASSERT_TRUE(t.has_value());
        text = *t;
    }
    EXPECT_EQ(text.find("ghp_secret"), std::string::npos);
    EXPECT_EQ(creds->items["github_token.ars-melpomene"], "ghp_secret");
    const auto loaded = store.load();
    EXPECT_TRUE(loaded.error.empty());
    EXPECT_EQ(loaded.config.github_token.value_or(""), "ghp_secret");
    fs::remove_all(f.parent_path());
}

TEST(StudioMelpomeneConfig, LegacyPlainTextTokenIsMigrated) {
    const fs::path f = temp_settings();
    std::string err;
    write_text_file(f, R"({"repository_owner":"acme","repository_name":"game","github_token":"old"})", err);
    auto creds = std::make_shared<FakeCreds>();
    const auto loaded = ConfigStore(f, creds).load();
    EXPECT_EQ(loaded.config.github_token.value_or(""), "old");
    EXPECT_EQ(creds->items["github_token.ars-melpomene"], "old");
    const auto text = read_text_file(f, err);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(text->find("old"), std::string::npos);

    write_text_file(f, "{broken", err);
    EXPECT_FALSE(ConfigStore(f, creds).load().error.empty());   // reported, not defaulted silently
    std::string e2;
    EXPECT_FALSE(merge_config(MelpomeneConfig{}, jobj({{"repository_owner", jstr("a/../b")}}), e2).has_value());
    fs::remove_all(f.parent_path());
}

TEST(StudioMelpomeneApi, IssuesSkipPullRequestsAndEncodeQueries) {
    auto http = std::make_shared<FakeHttp>();
    MelpomeneConfig c;
    c.repository_owner = "acme";
    c.repository_name  = "game";
    c.github_token     = "tok";
    http->routes["GET " + std::string(kBase) + "/issues?page=1&per_page=100&state=all&labels=a%20b%2Cc"] = {200, kIssues};
    GitHubApi api(http, c);
    const auto r = api.fetch_issues("all", {"a b", "c"}, 1, 100);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value->size(), 2u);
    EXPECT_EQ(http->header(0, "Authorization"), "Bearer tok");
    EXPECT_EQ(http->header(0, "User-Agent"), "ars-melpomene");

    http->routes["GET " + std::string(kBase) + "/issues/9"] = {404, "{\"message\":\"Not Found\"}"};
    const auto missing = api.fetch_issue(9);
    EXPECT_FALSE(missing.ok());
    EXPECT_EQ(missing.status, 404);

    GitHubApi unset(http, MelpomeneConfig{});
    EXPECT_EQ(unset.fetch_milestones().error.rfind("Not configured", 0), 0u);
}

TEST(StudioMelpomenePlugin, CachesIssuesAndFiltersByScenePosition) {
    const fs::path f = temp_settings();
    auto http  = std::make_shared<FakeHttp>();
    auto creds = std::make_shared<FakeCreds>();
    MelpomenePlugin p(ConfigStore(f, creds), http, quiet());
    EXPECT_TRUE(field(p.snapshot(), "ok")->as_bool());          // unconfigured is normal
    EXPECT_EQ(p.handle(req("GET"), "/api/issues").status, 503);

    std::string err;
    ASSERT_TRUE(p.update_config(jobj({{"repository_owner", jstr("acme")}, {"repository_name", jstr("game")},
                                      {"github_token", jstr("t")}}),
                                err));
    const JsonValue cfg = parse_json(p.handle(req("GET"), "/api/config").body);
    EXPECT_TRUE(field(cfg, "has_token")->as_bool());
    EXPECT_EQ(field(cfg, "github_token"), nullptr);              // never returned

    http->routes["GET " + std::string(kBase) + "/issues?page=1&per_page=100&state=all"] = {200, kIssues};
    ASSERT_EQ(p.handle(req("GET"), "/api/issues").status, 200);
    const std::size_t calls = http->seen.size();
    p.handle(req("GET"), "/api/issues");
    EXPECT_EQ(http->seen.size(), calls);                         // served from the cache
    net::HttpRequest r = req("GET");
    r.query = {{"refresh", "1"}};
    p.handle(r, "/api/issues");
    EXPECT_EQ(http->seen.size(), calls + 1);

    r.query = {{"scene", "Forest"}};
    EXPECT_EQ(parse_json(p.handle(r, "/api/issues/by-scene").body).a->size(), 1u);
    r.query = {{"x", "1"}, {"y", "2"}, {"z", "3.5"}, {"radius", "1"}};
    EXPECT_EQ(parse_json(p.handle(r, "/api/issues/near").body).a->size(), 1u);
    r.query = {{"x", "9"}, {"y", "9"}, {"z", "9"}, {"radius", "1"}};
    EXPECT_EQ(parse_json(p.handle(r, "/api/issues/near").body).a->size(), 0u);
    const JsonValue stats = parse_json(p.handle(req("GET"), "/api/cache-stats").body);
    EXPECT_EQ(field(stats, "open")->as_number(), 1.0);
    EXPECT_EQ(field(stats, "closed")->as_number(), 1.0);
    fs::remove_all(f.parent_path());
}

TEST(StudioMelpomenePlugin, NotificationsDetectNewReviewsAndRunChanges) {
    const fs::path f = temp_settings();
    auto http  = std::make_shared<FakeHttp>();
    auto creds = std::make_shared<FakeCreds>();
    MelpomenePlugin p(ConfigStore(f, creds), http, quiet());
    std::string err;
    ASSERT_TRUE(p.update_config(jobj({{"repository_owner", jstr("acme")}, {"repository_name", jstr("game")}}), err));
    const std::string pulls = "GET " + std::string(kBase) + "/pulls?state=open";
    const std::string revs  = "GET " + std::string(kBase) + "/pulls/7/reviews";
    const std::string runs  = "GET " + std::string(kBase) + "/actions/runs?per_page=10";
    http->routes[pulls] = {200, R"([{"number":7,"title":"Feature"}])"};
    http->routes[revs]  = {200, "[]"};
    http->routes[runs]  = {200, R"({"workflow_runs":[{"id":5,"name":"CI","status":"queued"}]})"};
    auto first = p.poll_notifications();
    ASSERT_TRUE(first.ok());
    EXPECT_FALSE(first.value->new_review_detected);

    http->routes[revs] = {200, R"([{"id":1,"state":"APPROVED","user":{"login":"r"}}])"};
    http->routes[runs] = {200, R"({"workflow_runs":[{"id":5,"name":"CI","status":"completed","conclusion":"success"}]})"};
    auto second = p.poll_notifications();
    ASSERT_TRUE(second.ok());
    EXPECT_TRUE(second.value->new_review_detected);
    EXPECT_TRUE(second.value->workflow_status_changed);
    EXPECT_EQ(second.value->pull_requests.at(0).review_count, 1u);
    fs::remove_all(f.parent_path());
}
