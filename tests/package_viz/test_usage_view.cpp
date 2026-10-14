// ergo_package_viz: P3 使用関係図 (usage)。 グループ、 実線と破線、 未観測の残し方、 根拠の札、
// 上限ぎりぎりと超過 (分割)、 validate_tela_graph 通過、 決定性。

#include "gtest/gtest.h"

#include "ergo/package_scan/scan.h"
#include "ergo/package_viz/groups.h"
#include "ergo/package_viz/usage_view.h"
#include "ergo/tela_graph/writer.h"

#include <set>
#include <string>

using namespace ergo;
using namespace ergo::package_viz;

namespace {

const std::string kScanFixtures = ERGO_PACKAGE_SCAN_FIXTURES;

package_scan::UsageModel scan_fixture() {
    package_scan::ScanOptions o;
    o.manifest_path = kScanFixtures + "/ergo/ergo-packages.json";
    o.root          = kScanFixtures + "/root";
    package_scan::UsageModel m;
    package_scan::scan_usage(o, m);
    return m;
}

const graph_layout::LayoutNode* node_of(const ViewGraph& v, const std::string& id) {
    for (const auto& n : v.graph.nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const graph_layout::LayoutEdge* edge_of(const ViewGraph& v, const std::string& from,
                                        const std::string& to) {
    for (const auto& e : v.graph.edges) {
        if (e.from == from && e.to == to) return &e;
    }
    return nullptr;
}

/// n 個の観測ノードと、 そこから 1 つの共有ノードへの辺を持つ合成の UsageModel。
package_scan::UsageModel synthetic(std::size_t observed) {
    package_scan::UsageModel m;
    m.nodes.push_back({"npm:@ludiars/core", "@ludiars/core", "", package_scan::UsageKind::SharedTs,
                       "", "", ""});
    for (std::size_t i = 0; i < observed; ++i) {
        const std::string id = "obs" + std::to_string(1000 + i);
        m.nodes.push_back({id, id, "", package_scan::UsageKind::Musa, "musa", "", ""});
        package_scan::UsageEdge e;
        e.from = id;
        e.to   = "npm:@ludiars/core";
        e.kinds.push_back(package_scan::UsageEdgeKind::ScopedDep);
        e.evidence.push_back({id + "/package.json", 3, "dependencies.@ludiars/core", "^1"});
        m.edges.push_back(e);
    }
    return m;
}

} // namespace

TEST(UsageView, GroupsFollowP3) {
    const auto view = build_usage_view(scan_fixture());
    EXPECT_EQ(node_of(view, "thaleia")->group_id, std::string(kGroupMusa));
    EXPECT_EQ(node_of(view, "clio")->group_id, std::string(kGroupMusa));
    EXPECT_EQ(node_of(view, "curare")->group_id, std::string(kGroupRelated));
    EXPECT_EQ(node_of(view, "lapilli")->group_id, std::string(kGroupSharedTs));
    EXPECT_EQ(node_of(view, "ergo")->group_id, std::string(kGroupErgo));
    EXPECT_EQ(node_of(view, "ergo_resource")->group_id, std::string(kGroupErgo));
    EXPECT_EQ(node_of(view, "tools/ergo")->group_id, std::string(kGroupTool));
    EXPECT_EQ(node_of(view, "ars-musa")->group_id, std::string(kGroupUnobserved));
    EXPECT_LE(usage_groups().size(), static_cast<std::size_t>(tela_graph::kTelaMaxGroups));
}

TEST(UsageView, ObservedEntriesWithoutEdgesAreStillDrawn) {
    const auto view = build_usage_view(scan_fixture());
    ASSERT_TRUE(node_of(view, "clio") != nullptr);   // 移植の破線だけ
    ASSERT_TRUE(node_of(view, "ars-musa") != nullptr);   // 未観測・辺なし
    EXPECT_TRUE(node_of(view, "ars-musa")->label.find("未観測: チェックアウトなし") != std::string::npos);
    // ars-musa (チェックアウトなし) と escape (path_hint が範囲外) の 2 つ。
    bool noted = false;
    for (const auto& n : view.notes) noted = noted || n == "未観測 2";
    EXPECT_TRUE(noted);
}

TEST(UsageView, SolidForLinksDashedForPortsAndVocabulary) {
    const auto view = build_usage_view(scan_fixture());
    ASSERT_TRUE(edge_of(view, "thaleia", "lapilli") != nullptr);
    EXPECT_FALSE(edge_of(view, "thaleia", "lapilli")->dashed);
    ASSERT_TRUE(edge_of(view, "ergo", "lapilli") != nullptr);
    EXPECT_FALSE(edge_of(view, "ergo", "lapilli")->dashed);
    ASSERT_TRUE(edge_of(view, "ergo_resource", "clio") != nullptr);
    EXPECT_TRUE(edge_of(view, "ergo_resource", "clio")->dashed);
    ASSERT_TRUE(edge_of(view, "ergo_resource", "curare") != nullptr);
    EXPECT_TRUE(edge_of(view, "ergo_resource", "curare")->dashed);
}

TEST(UsageView, EvidenceIsOnTheUsingNodeLabel) {
    const auto view = build_usage_view(scan_fixture());
    const auto& label = node_of(view, "thaleia")->label;
    EXPECT_TRUE(label.find("Thaleia/.gitmodules:4 submodule.lib/lapilli.url") != std::string::npos);
    EXPECT_TRUE(label.find("Thaleia/package.json:5") != std::string::npos);
    const auto& port = node_of(view, "ergo_resource")->label;
    EXPECT_TRUE(port.find("ergo:ergo-packages.json:10") != std::string::npos);
}

TEST(UsageView, EvidenceLinesAreCapped) {
    auto m = synthetic(1);
    for (int i = 0; i < 5; ++i) {
        m.edges[0].evidence.push_back({"obs1000/package.json", 4 + i, "k", ""});
    }
    const auto view = build_usage_view(m);
    EXPECT_TRUE(node_of(view, "obs1000")->label.find("…他 3 件") != std::string::npos);
}

TEST(UsageView, FitsAndPassesTelaValidation) {
    FitResult result;
    std::string error;
    ASSERT_TRUE(make_usage_view(scan_fixture(), ViewOptions{}, result, &error));
    ASSERT_EQ(result.graphs.size(), 1u);
    EXPECT_FALSE(result.split);
    EXPECT_TRUE(tela_graph::validate_tela_graph(result.graphs[0], &error));
    std::set<std::string> groups;
    for (const auto& n : result.graphs[0].nodes) groups.insert(n.group_id);
    EXPECT_LE(groups.size(), static_cast<std::size_t>(tela_graph::kTelaMaxGroups));
}

TEST(UsageView, LimitEdgeIsOneGraph) {
    // 共有ノード 1 + 観測 127 = 128 ノード (上限ちょうど)。
    FitResult result;
    std::string error;
    ASSERT_TRUE(make_usage_view(synthetic(tela_graph::kTelaMaxNodes - 1), ViewOptions{}, result, &error));
    EXPECT_FALSE(result.split);
    ASSERT_EQ(result.graphs.size(), 1u);
    EXPECT_EQ(result.graphs[0].nodes.size(), static_cast<std::size_t>(tela_graph::kTelaMaxNodes));
}

TEST(UsageView, OverLimitIsSplitAndEveryPartValidates) {
    FitResult result;
    std::string error;
    ASSERT_TRUE(make_usage_view(synthetic(tela_graph::kTelaMaxNodes + 20), ViewOptions{}, result, &error));
    EXPECT_TRUE(result.split);
    EXPECT_GT(result.graphs.size(), 1u);
    for (const auto& g : result.graphs) {
        EXPECT_TRUE(tela_graph::validate_tela_graph(g, &error));
        EXPECT_LE(g.nodes.size(), static_cast<std::size_t>(tela_graph::kTelaMaxNodes));
    }
    EXPECT_TRUE(tela_graph::validate_tela_graph(result.index, &error));
    const auto files = output_files(result, "usage.tela");
    EXPECT_EQ(files.back().first, std::string("usage.index.tela"));
}

TEST(UsageView, FocusKeepsNeighbours) {
    ViewOptions o;
    o.focus = {"thaleia"};
    FitResult result;
    std::string error;
    ASSERT_TRUE(make_usage_view(scan_fixture(), o, result, &error));
    std::set<std::string> ids;
    for (const auto& n : result.graphs[0].nodes) ids.insert(n.id);
    EXPECT_TRUE(ids.count("thaleia") == 1);
    EXPECT_TRUE(ids.count("lapilli") == 1);
    EXPECT_TRUE(ids.count("terpsichore") == 1);
    EXPECT_TRUE(ids.count("ars-musa") == 0);
}

TEST(UsageView, Deterministic) {
    FitResult a, b;
    ASSERT_TRUE(make_usage_view(scan_fixture(), ViewOptions{}, a));
    ASSERT_TRUE(make_usage_view(scan_fixture(), ViewOptions{}, b));
    EXPECT_EQ(tela_graph::to_tela_graph_text(a.graphs[0]), tela_graph::to_tela_graph_text(b.graphs[0]));
}

TEST(UsageView, MakeViewRefusesUsageWithoutScan) {
    ViewOptions o;
    o.view = View::Usage;
    FitResult result;
    std::string error;
    EXPECT_FALSE(make_view(package_model::PackageModel{}, o, result, &error));
}
