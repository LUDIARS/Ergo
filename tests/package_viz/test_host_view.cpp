// ergo_package_viz: P4 ホスト状態図 (host、 spec/feature/package-manager.md §5.1 / §7 W6)。
// resolved + 報告 + JUnit 由来の状態の見本から作った図が validate_tela_graph を通る。 グループ・辺の種類・札、
// 審査中 20 件の由来ごとの畳み込み、 入力が欠けたときの "—"、 決定性。

#include "gtest/gtest.h"

#include "ergo/package_model/host_state_reader.h"
#include "ergo/package_viz/groups.h"
#include "ergo/package_viz/host_view.h"
#include "ergo/tela_graph/writer.h"

#include <set>
#include <string>

using namespace ergo;
using namespace ergo::package_viz;

namespace {

const std::string kHostDir = std::string(ERGO_PACKAGE_MODEL_FIXTURES) + "/host_state";

package_model::HostState fixture_state() {
    return package_model::load_host_state({kHostDir + "/ergo-packages.resolved.json",
                                           kHostDir + "/watch-report.json",
                                           kHostDir + "/ergo-packages.status.json"});
}

const graph_layout::LayoutNode* node_of(const ViewGraph& v, const std::string& id) {
    for (const auto& n : v.graph.nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const graph_layout::LayoutEdge* edge_of(const ViewGraph& v, const std::string& from, const std::string& to) {
    for (const auto& e : v.graph.edges) {
        if (e.from == from && e.to == to) return &e;
    }
    return nullptr;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

/// 依存 pictor に review / run を n 件持つ報告 (由来は pf / ai / human / unknown を順に回す)。
package_model::HostState many_reviews(std::size_t n) {
    package_model::HostState s;
    s.has_report  = true;
    s.report.host = "Example";
    package_model::WatchDependency d;
    d.name     = "pictor";
    d.code     = "Pc";
    d.observed = true;
    const char* origins[] = {"pf", "ai", "human", "unknown"};
    for (std::size_t i = 0; i < n; ++i) {
        package_model::WatchUpdate u;
        u.dependency = "pictor";
        u.kind       = i % 5 == 4 ? "run" : "review";
        u.origin     = origins[i % 4];
        if (u.kind == "review") {
            u.review  = static_cast<long long>(200 + i);
            u.head    = "abcdef0";
            u.subject = "#" + std::to_string(200 + i) + "@abcdef0";
        } else {
            u.subject = "run-" + std::to_string(i);
        }
        d.updates.push_back(u);
    }
    s.report.dependencies.push_back(d);
    return s;
}

} // namespace

TEST(PackageVizHostView, FixtureNodesEdgesAndLabels) {
    const auto v = build_host_view(fixture_state());
    EXPECT_TRUE(v.graph.groups.size() <= 8u);
    EXPECT_EQ(v.graph.title, std::string("ホスト状態図 Example"));

    const auto* host = node_of(v, "host");
    ASSERT_TRUE(host != nullptr);
    EXPECT_EQ(host->group_id, std::string(kGroupHost));
    EXPECT_EQ(host->label, std::string("Example | HEAD 1a2b3c4 | モード M1 | テスト 1 失敗 (5 中)"));

    const auto* ergo = node_of(v, "ergo");
    ASSERT_TRUE(ergo != nullptr);
    EXPECT_TRUE(contains(ergo->label, "版 v0.9.0"));
    EXPECT_TRUE(contains(ergo->label, "pin 7ea616e"));
    EXPECT_TRUE(contains(ergo->label, "main +18"));
    EXPECT_TRUE(contains(ergo->label, "新タグ v0.10.0"));

    const auto* pictor = node_of(v, "dep:pictor");
    ASSERT_TRUE(pictor != nullptr);
    EXPECT_EQ(pictor->group_id, std::string(kGroupDependency));
    EXPECT_TRUE(contains(pictor->label, "pictor (Pc)"));
    EXPECT_TRUE(contains(pictor->label, "版 v2.2.0 · pin 625d83d"));
    EXPECT_TRUE(contains(pictor->label, "経路 provided · drift ahead"));
    EXPECT_TRUE(contains(pictor->label, "機能 vulkan"));
    EXPECT_TRUE(contains(pictor->label, "★drift dirty"));
    // resolved.json で absent の依存は描かない。 報告にだけある非公開の依存は描く。
    EXPECT_TRUE(node_of(v, "dep:tela") == nullptr);
    const auto* priv = node_of(v, "dep:examplepriv");
    ASSERT_TRUE(priv != nullptr);
    EXPECT_TRUE(contains(priv->label, "未観測"));
    EXPECT_TRUE(contains(priv->label, "非公開"));

    const auto* update = node_of(v, "update:pictor");
    ASSERT_TRUE(update != nullptr);
    EXPECT_EQ(update->group_id, std::string(kGroupUpdate));
    EXPECT_TRUE(contains(update->label, "v2.3.0 (41 先)"));
    EXPECT_TRUE(contains(update->label, "main +41 (うち merged 3)"));
    EXPECT_TRUE(node_of(v, "update:ergo") != nullptr);
    EXPECT_TRUE(node_of(v, "update:examplepriv") == nullptr);

    const auto* pf = node_of(v, "review:pictor:#128@4f0e9a1");
    ASSERT_TRUE(pf != nullptr);
    EXPECT_EQ(pf->group_id, std::string(kGroupReview));
    EXPECT_TRUE(contains(pf->label, "#128 Pf"));
    EXPECT_TRUE(contains(node_of(v, "review:pictor:#131@5a6b7c8")->label, "#131 AI"));
    EXPECT_TRUE(contains(node_of(v, "review:pictor:#133@9d8e7f6")->label, "#133 人"));
    EXPECT_TRUE(contains(node_of(v, "review:pictor:run-0001")->label, "run AI"));

    // 落ちた検査 (error / warning) だけ。 record は描かない。 対象が依存でなければ host から。
    EXPECT_TRUE(node_of(v, "check:PKG-05:pictor") != nullptr);
    EXPECT_TRUE(node_of(v, "check:PKG-15:Example") != nullptr);
    EXPECT_TRUE(node_of(v, "check:PKG-07:pictor") == nullptr);

    // 辺: host → Ergo・依存は実線、 依存 → 更新・審査中・検査は破線。
    ASSERT_TRUE(edge_of(v, "host", "ergo") != nullptr);
    EXPECT_FALSE(edge_of(v, "host", "ergo")->dashed);
    ASSERT_TRUE(edge_of(v, "host", "dep:pictor") != nullptr);
    EXPECT_FALSE(edge_of(v, "host", "dep:pictor")->dashed);
    ASSERT_TRUE(edge_of(v, "dep:pictor", "update:pictor") != nullptr);
    EXPECT_TRUE(edge_of(v, "dep:pictor", "update:pictor")->dashed);
    ASSERT_TRUE(edge_of(v, "ergo", "update:ergo") != nullptr);
    ASSERT_TRUE(edge_of(v, "dep:pictor", "review:pictor:#128@4f0e9a1") != nullptr);
    ASSERT_TRUE(edge_of(v, "dep:pictor", "check:PKG-05:pictor") != nullptr);
    EXPECT_TRUE(edge_of(v, "dep:pictor", "check:PKG-05:pictor")->dashed);
    ASSERT_TRUE(edge_of(v, "host", "check:PKG-15:Example") != nullptr);
}

TEST(PackageVizHostView, FixtureGraphPassesTelaValidation) {
    FitResult r;
    std::string error;
    ASSERT_TRUE(make_host_view(fixture_state(), ViewOptions{}, r, &error));
    EXPECT_FALSE(r.split);
    ASSERT_EQ(r.graphs.size(), 1u);
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.graphs[0], &error));
}

TEST(PackageVizHostView, TwentyReviewsFoldByOrigin) {
    const auto state = many_reviews(20);
    const auto v     = build_host_view(state);
    std::size_t review_nodes = 0;
    for (const auto& n : v.graph.nodes) review_nodes += n.group_id == kGroupReview ? 1 : 0;
    EXPECT_EQ(review_nodes, 4u);
    EXPECT_TRUE(review_nodes <= kHostReviewNodes);
    const auto* pf = node_of(v, "review:origin:pf");
    ASSERT_TRUE(pf != nullptr);
    EXPECT_TRUE(contains(pf->label, "審査中・作業中 Pf | 5 件"));
    ASSERT_TRUE(node_of(v, "review:origin:human") != nullptr);
    EXPECT_TRUE(edge_of(v, "dep:pictor", "review:origin:ai") != nullptr);
    EXPECT_TRUE(contains(v.notes.back(), "20 件を由来ごとに畳み込み") ||
                contains(v.notes.front(), "20 件を由来ごとに畳み込み"));

    FitResult r;
    std::string error;
    ASSERT_TRUE(make_host_view(state, ViewOptions{}, r, &error));
    ASSERT_EQ(r.graphs.size(), 1u);
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.graphs[0], &error));

    // 上限ちょうどは畳まない。
    const auto at_limit = build_host_view(many_reviews(kHostReviewNodes));
    EXPECT_TRUE(node_of(at_limit, "review:origin:pf") == nullptr);
}

TEST(PackageVizHostView, MissingInputsStillDraw) {
    const package_model::HostState empty;
    const auto v = build_host_view(empty);
    ASSERT_EQ(v.graph.nodes.size(), 2u);
    const std::string dash = kHostMissing;
    EXPECT_EQ(node_of(v, "host")->label,
              dash + " | HEAD " + dash + " | モード " + dash + " | テスト " + dash);
    EXPECT_TRUE(contains(node_of(v, "ergo")->label, "pin " + dash));

    FitResult r;
    std::string error;
    ASSERT_TRUE(make_host_view(empty, ViewOptions{}, r, &error));
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.graphs[0], &error));

    // 状態だけ欠けた: テストは "—"、 状態の source が unknown なら "未実行"。
    auto no_status = fixture_state();
    no_status.has_status = false;
    EXPECT_TRUE(contains(build_host_view(no_status).graph.nodes[0].label, "テスト " + dash));
    auto not_run = fixture_state();
    not_run.status.tests = package_model::TestSummary{};
    EXPECT_TRUE(contains(build_host_view(not_run).graph.nodes[0].label, "テスト 未実行"));
}

TEST(PackageVizHostView, Deterministic) {
    FitResult a, b;
    ASSERT_TRUE(make_host_view(fixture_state(), ViewOptions{}, a));
    ASSERT_TRUE(make_host_view(fixture_state(), ViewOptions{}, b));
    ASSERT_EQ(a.graphs.size(), b.graphs.size());
    EXPECT_EQ(tela_graph::to_tela_graph_text(a.graphs[0]), tela_graph::to_tela_graph_text(b.graphs[0]));
}
