// 由来 (spec/feature/package-manager.md §3「由来」 / §7 W4): local PR 見本 + run 見本の突合で pf / ai (trigger 写し) /
// human / unknown。 open → merged で merged が 1 回。 Revisor / Concordia が落ちていると run / review を出さず
// unavailable。 loopback 以外の URL を拒む。 出来事に PR のタイトル・本文・ブランチ名が入らない (見本にわざと入れてある)。
// ネットワークは使わない (Revisor / Concordia の応答は JSON 見本)。

#include "fake_source.h"

#include "ergo/package_watch/loopback_url.h"
#include "ergo/package_watch/provenance.h"
#include "ergo/package_watch/provenance_json.h"
#include "ergo/package_watch/watch_report.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace ergo::package_watch;
using ergo::package_watch::test::dependency;

namespace jsonm = ergo::common::jsonm;

namespace {

constexpr std::int64_t kT0 = 1792022400;  // 2026-10-15T00:00:00Z

std::string read_fixture(const std::string& name) {
    std::ifstream in(std::string(ERGO_PACKAGE_WATCH_FIXTURES) + "/" + name, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

ReviewListing reviews_from(const char* open, const char* merged) {
    ReviewListing l;
    std::string error;
    l.available = parse_local_prs(read_fixture(open), l.open, &error);
    if (merged) l.available = l.available && parse_local_prs(read_fixture(merged), l.merged, &error);
    return l;
}

RunListing runs_from(const char* name) {
    RunListing l;
    std::string error;
    l.available = parse_delegation_runs(read_fixture(name), l.runs, &error);
    return l;
}

WatchDependency pictor() { return dependency("pictor", 1, "v2.2.0"); }

/// ref は見ていない (observation.observed = false) 結果に由来だけを足す。
std::vector<DependencyResult> provenance_results(const ProvenanceInputs& in, const WatchState& prev) {
    std::vector<DependencyResult> results;
    DependencyResult r;
    r.dependency = pictor();
    results.push_back(r);
    attach_provenance(results, in, prev);
    return results;
}

const Candidate* find_update(const std::vector<Candidate>& updates, UpdateKind kind, const std::string& subject) {
    for (const auto& c : updates) {
        if (c.kind == kind && c.subject == subject) return &c;
    }
    return nullptr;
}

std::size_t count_kind(const std::vector<WatchEvent>& events, UpdateKind kind) {
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(),
                                                  [&](const WatchEvent& e) { return e.update.kind == kind; }));
}

std::size_t count_entries(const WatchState& state, UpdateKind kind) {
    return static_cast<std::size_t>(std::count_if(state.entries.begin(), state.entries.end(),
                                                  [&](const StateEntry& e) { return e.update.kind == kind; }));
}

/// 1 回目: open な #128 / #131 / #133 と進行中の run 2 本。
DiffResult first_round(std::vector<DependencyResult>* out_results = nullptr) {
    const auto reviews = reviews_from("revisor-open.json", nullptr);
    const auto runs    = runs_from("concordia-runs.json");
    auto results       = provenance_results({&reviews, &runs}, WatchState{});
    auto d             = diff(WatchState{}, "Example", results, kT0);
    if (out_results) *out_results = results;
    return d;
}

} // namespace

TEST(PackageWatchProvenance, ParsesListings) {
    const auto reviews = reviews_from("revisor-open.json", "revisor-merged.json");
    ASSERT_TRUE(reviews.available);
    ASSERT_EQ(reviews.open.size(), static_cast<std::size_t>(5));
    EXPECT_EQ(reviews.open[0].number, 128);
    EXPECT_EQ(reviews.open[0].session_id, std::string("sess-pf-child"));
    EXPECT_EQ(reviews.open[1].session_id, std::string(""));  // null は空
    ASSERT_EQ(reviews.merged.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(reviews.merged[0].merge_sha, std::string("9d8c7b6000000000000000000000000000000128"));

    const auto runs = runs_from("concordia-runs.json");
    ASSERT_TRUE(runs.available);
    ASSERT_EQ(runs.runs.size(), static_cast<std::size_t>(5));
    EXPECT_EQ(runs.runs[0].repository, std::string("pictor"));  // sessions[].repo_origin から
    EXPECT_EQ(runs.runs[1].repository, std::string("pictor"));  // args.target_repo の末尾名から
    EXPECT_EQ(runs.runs[4].repository, std::string("tela"));

    std::vector<LocalPr> prs;
    std::string error;
    EXPECT_FALSE(parse_local_prs("{\"runs\": []}", prs, &error));
    EXPECT_FALSE(parse_local_prs("not json", prs, &error));
    std::vector<DelegationRun> rs;
    EXPECT_FALSE(parse_delegation_runs("{\"pullRequests\": []}", rs, &error));
}

TEST(PackageWatchProvenance, RepoKey) {
    EXPECT_EQ(repo_key("LUDIARS/Pictor"), std::string("pictor"));
    EXPECT_EQ(repo_key("https://github.com/LUDIARS/Pictor.git"), std::string("pictor"));
    EXPECT_EQ(repo_key("repos\\Pictor\\"), std::string("pictor"));
    EXPECT_EQ(repo_key("git@github.com:Pictor.git"), std::string("pictor"));
}

TEST(PackageWatchProvenance, OriginMatrix) {
    const auto reviews = reviews_from("revisor-open.json", nullptr);
    const auto runs    = runs_from("concordia-runs.json");

    // #128: sessionId が Pf の run の child_session_id と一致 → pf
    auto o = classify_review(reviews.open[0], &runs);
    EXPECT_EQ(o.origin, std::string(kOriginPf));
    EXPECT_EQ(o.trigger, std::string(""));
    // #131: sessionId 無し、 headRef == spawn_branch かつ同じリポ → ai (trigger を写す)
    o = classify_review(reviews.open[1], &runs);
    EXPECT_EQ(o.origin, std::string(kOriginAi));
    EXPECT_EQ(o.trigger, std::string("claude-cc"));
    // #133: 突合する run が無い → human
    o = classify_review(reviews.open[2], &runs);
    EXPECT_EQ(o.origin, std::string(kOriginHuman));
    // Concordia に届かない / 見ていない → unknown
    RunListing down;
    down.reason = "not reachable";
    EXPECT_EQ(classify_review(reviews.open[0], &down).origin, std::string(kOriginUnknown));
    EXPECT_EQ(classify_review(reviews.open[0], nullptr).origin, std::string(kOriginUnknown));
}

TEST(PackageWatchProvenance, DetectsRunsAndReviews) {
    const auto reviews = reviews_from("revisor-open.json", nullptr);
    const auto runs    = runs_from("concordia-runs.json");
    const auto p       = detect_provenance(pictor(), {&reviews, &runs}, {});
    EXPECT_TRUE(p.reviews_observed);
    EXPECT_TRUE(p.runs_observed);

    // run: Pictor を対象にした進行中の run だけ (completed と別リポは出ない)
    const auto* ai_run = find_update(p.updates, UpdateKind::Run, "run-ai-active");
    ASSERT_TRUE(ai_run != nullptr);
    EXPECT_EQ(ai_run->origin, std::string(kOriginAi));
    EXPECT_EQ(ai_run->trigger, std::string("claude-cc"));
    const auto* pf_run = find_update(p.updates, UpdateKind::Run, "run-pf-queued");
    ASSERT_TRUE(pf_run != nullptr);
    EXPECT_EQ(pf_run->origin, std::string(kOriginPf));
    EXPECT_TRUE(find_update(p.updates, UpdateKind::Run, "run-pf-done") == nullptr);
    EXPECT_TRUE(find_update(p.updates, UpdateKind::Run, "run-other-repo") == nullptr);

    // review: 同じリポで base が追跡ブランチ (main) の open な PR。 主体は番号と head SHA7
    const auto* pf_pr = find_update(p.updates, UpdateKind::Review, "#128@4f0e9a1");
    ASSERT_TRUE(pf_pr != nullptr);
    EXPECT_EQ(pf_pr->origin, std::string(kOriginPf));
    ASSERT_TRUE(pf_pr->review.has_value());
    EXPECT_EQ(*pf_pr->review, 128);
    EXPECT_EQ(pf_pr->head, std::string("4f0e9a1"));
    EXPECT_TRUE(find_update(p.updates, UpdateKind::Review, "#131@5a1b2c3") != nullptr);
    EXPECT_TRUE(find_update(p.updates, UpdateKind::Review, "#133@6c7d8e9") != nullptr);
    std::size_t reviews_seen = 0;
    for (const auto& c : p.updates) reviews_seen += c.kind == UpdateKind::Review ? 1 : 0;
    EXPECT_EQ(reviews_seen, static_cast<std::size_t>(3));  // 別リポ (#140) と base が develop (#141) は出ない
}

TEST(PackageWatchProvenance, OpenToMergedOnce) {
    const auto d1 = first_round();
    EXPECT_EQ(count_kind(d1.events, UpdateKind::Review), static_cast<std::size_t>(3));
    EXPECT_EQ(count_kind(d1.events, UpdateKind::Run), static_cast<std::size_t>(2));
    EXPECT_EQ(count_entries(d1.next, UpdateKind::Review), static_cast<std::size_t>(3));

    // 2 回目: #128 が open から消えて merged に入った
    auto reviews = reviews_from("revisor-open.json", "revisor-merged.json");
    reviews.open.erase(reviews.open.begin());
    const auto runs = runs_from("concordia-runs.json");
    const auto r2   = provenance_results({&reviews, &runs}, d1.next);
    const auto d2   = diff(d1.next, "Example", r2, kT0 + 900);
    ASSERT_EQ(count_kind(d2.events, UpdateKind::Merged), static_cast<std::size_t>(1));
    EXPECT_EQ(d2.events.size(), static_cast<std::size_t>(1));  // 既知の review / run は再び出ない
    const auto& merged = d2.events[0];
    EXPECT_EQ(merged.id, std::string("Example/pictor/merged/#128"));
    EXPECT_EQ(merged.update.origin, std::string(kOriginPf));  // 由来は前回の review から写す
    EXPECT_EQ(merged.update.head, std::string("4f0e9a1"));
    EXPECT_EQ(merged.update.to_rev, std::string("9d8c7b6000000000000000000000000000000128"));
    EXPECT_EQ(count_entries(d2.next, UpdateKind::Review), static_cast<std::size_t>(2));
    EXPECT_EQ(count_entries(d2.next, UpdateKind::Merged), static_cast<std::size_t>(0));  // 状態には残さない

    // 3 回目: 同じ応答でも merged は 2 度出ない (#90 は前回 review でないので出ない)
    const auto r3 = provenance_results({&reviews, &runs}, d2.next);
    const auto d3 = diff(d2.next, "Example", r3, kT0 + 1800);
    EXPECT_EQ(d3.events.size(), static_cast<std::size_t>(0));
}

TEST(PackageWatchProvenance, UnavailableSourcesKeepState) {
    const auto d1 = first_round();

    ReviewListing reviews_down;
    reviews_down.reason = "not reachable";
    RunListing runs_down;
    runs_down.reason = "HTTP 401";
    const ProvenanceInputs down{&reviews_down, &runs_down};
    const auto r2 = provenance_results(down, d1.next);
    EXPECT_FALSE(r2[0].reviews_observed);
    EXPECT_FALSE(r2[0].runs_observed);
    EXPECT_TRUE(r2[0].updates.empty());  // run / review を出さない

    const auto d2 = diff(d1.next, "Example", r2, kT0 + 900);
    EXPECT_TRUE(d2.events.empty());
    // 届かなかった取得元の既知はそのまま (次に届いたときに merged を判定できる)
    EXPECT_EQ(jsonm::serialize(state_to_json(d2.next)), jsonm::serialize(state_to_json(d1.next)));

    const auto sources = source_states(down);
    EXPECT_EQ(sources.revisor, std::string(kSourceUnavailable));
    EXPECT_EQ(sources.concordia, std::string(kSourceUnavailable));
    EXPECT_EQ(source_states({}).revisor, std::string(kSourceOff));

    HostWatch w;
    w.host    = "Example";
    w.results = r2;
    w.diff    = d2;
    w.sources = sources;
    const auto out = prepare_watch_outputs({w}, kT0 + 900, sources);
    ASSERT_TRUE(out.ok);
    EXPECT_TRUE(out.reports[0].content.find("\"sources\":{\"concordia\":\"unavailable\",\"revisor\":\"unavailable\"}") !=
                std::string::npos);
    EXPECT_TRUE(out.events.content.find("\"revisor\":\"unavailable\"") != std::string::npos);
}

TEST(PackageWatchProvenance, ConcordiaDownMakesOriginUnknown) {
    const auto reviews = reviews_from("revisor-open.json", nullptr);
    RunListing runs_down;
    runs_down.reason = "not reachable";
    const auto p = detect_provenance(pictor(), {&reviews, &runs_down}, {});
    EXPECT_TRUE(p.reviews_observed);
    EXPECT_FALSE(p.runs_observed);
    std::size_t reviews_seen = 0;
    for (const auto& c : p.updates) {
        EXPECT_TRUE(c.kind != UpdateKind::Run);
        if (c.kind == UpdateKind::Review) {
            ++reviews_seen;
            EXPECT_EQ(c.origin, std::string(kOriginUnknown));
        }
    }
    EXPECT_EQ(reviews_seen, static_cast<std::size_t>(3));
}

TEST(PackageWatchProvenance, EventsCarryNoTitleBodyOrBranch) {
    std::vector<DependencyResult> results;
    const auto d1 = first_round(&results);
    auto reviews  = reviews_from("revisor-open.json", "revisor-merged.json");
    reviews.open.erase(reviews.open.begin());
    const auto runs = runs_from("concordia-runs.json");
    const auto r2   = provenance_results({&reviews, &runs}, d1.next);
    const auto d2   = diff(d1.next, "Example", r2, kT0 + 900);

    HostWatch w1{"Example", results, d1, source_states({&reviews, &runs})};
    HostWatch w2{"Example", r2, d2, source_states({&reviews, &runs})};
    std::string all;
    for (const auto& w : {w1, w2}) {
        const auto out = prepare_watch_outputs({w}, kT0, w.sources);
        ASSERT_TRUE(out.ok);
        all += out.reports[0].content + out.events.content;
    }
    all += jsonm::serialize(state_to_json(d1.next)) + jsonm::serialize(state_to_json(d2.next));
    EXPECT_TRUE(all.find("\"kind\":\"merged\"") != std::string::npos);  // 確かに出来事を書いている
    for (const char* banned : {"TITLE-MARKER", "BODY-MARKER", "branch-marker", "feat/", "fix/", "sess-",
                               "SUMMARY-MARKER", "repos/", "github.com"}) {
        EXPECT_TRUE(all.find(banned) == std::string::npos);
    }
}

TEST(PackageWatchProvenance, StateRoundTripKeepsOrigin) {
    const auto d1  = first_round();
    const auto doc = state_to_json(d1.next);
    WatchState back;
    std::string error;
    ASSERT_TRUE(read_state(doc, back, &error));
    EXPECT_EQ(jsonm::serialize(state_to_json(back)), jsonm::serialize(doc));
    const auto prev = previous_reviews(back, "pictor");
    ASSERT_EQ(prev.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(*prev[0].review, 128);
    EXPECT_EQ(prev[0].origin, std::string(kOriginPf));
}

TEST(PackageWatchProvenance, DeterministicOverInputOrder) {
    const auto reviews = reviews_from("revisor-open.json", nullptr);
    const auto runs    = runs_from("concordia-runs.json");
    auto reviews_rev   = reviews;
    auto runs_rev      = runs;
    std::reverse(reviews_rev.open.begin(), reviews_rev.open.end());
    std::reverse(runs_rev.runs.begin(), runs_rev.runs.end());
    const auto a = diff(WatchState{}, "Example", provenance_results({&reviews, &runs}, WatchState{}), kT0);
    const auto b = diff(WatchState{}, "Example", provenance_results({&reviews_rev, &runs_rev}, WatchState{}), kT0);
    HostWatch wa{"Example", {}, a, {}};
    HostWatch wb{"Example", {}, b, {}};
    EXPECT_EQ(jsonm::serialize(events_json({wa}, kT0)), jsonm::serialize(events_json({wb}, kT0)));
    EXPECT_EQ(jsonm::serialize(state_to_json(a.next)), jsonm::serialize(state_to_json(b.next)));
}

TEST(PackageWatchProvenance, RefKindsKeepTheirW3Shape) {
    Candidate c;
    c.dependency = "pictor";
    c.kind       = UpdateKind::Tag;
    c.subject    = "v2.3.0";
    const std::string text = jsonm::serialize(candidate_to_json(c));
    EXPECT_TRUE(text.find("origin") == std::string::npos);
    EXPECT_TRUE(text.find("review") == std::string::npos);
}

TEST(PackageWatchLoopback, AcceptsOnlyLoopback) {
    EXPECT_TRUE(is_loopback_url("http://127.0.0.1:1234"));
    EXPECT_TRUE(is_loopback_url("http://localhost/"));
    EXPECT_TRUE(is_loopback_url("HTTP://LOCALHOST:80/v1"));
    EXPECT_TRUE(is_loopback_url("http://[::1]:8080/x"));
    EXPECT_TRUE(is_loopback_url("https://127.0.0.5"));
    EXPECT_FALSE(is_loopback_url(""));
    EXPECT_FALSE(is_loopback_url("http://example.com"));
    EXPECT_FALSE(is_loopback_url("http://127.0.0.1.example.com/"));
    EXPECT_FALSE(is_loopback_url("http://user@127.0.0.1/"));
    EXPECT_FALSE(is_loopback_url("http://127.0.0.1@example.com/"));
    EXPECT_FALSE(is_loopback_url("ftp://127.0.0.1/"));
    EXPECT_FALSE(is_loopback_url("http://10.0.0.1/"));
    EXPECT_FALSE(is_loopback_url("http://localhost:abc/"));
    EXPECT_FALSE(is_loopback_url("http://127.0.0.256/"));
    EXPECT_FALSE(is_loopback_url("http://[::2]/"));
    EXPECT_FALSE(is_loopback_url("127.0.0.1:1234"));
}
