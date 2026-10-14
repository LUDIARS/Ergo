// ergo_package_viz: P1 / P2 の組み立て、 推移簡約、 焦点と畳み込み、 分割と索引、
// 上限ぎりぎりと超過、 validate_tela_graph 通過、 決定性。

#include "gtest/gtest.h"

#include "ergo/package_model/loader.h"
#include "ergo/package_viz/collapse_focus.h"
#include "ergo/package_viz/fit.h"
#include "ergo/package_viz/groups.h"
#include "ergo/package_viz/package_view.h"
#include "ergo/package_viz/reduction.h"
#include "ergo/package_viz/split.h"
#include "ergo/package_viz/target_view.h"
#include "ergo/tela_graph/writer.h"

#include <set>
#include <string>

using namespace ergo;
using namespace ergo::package_viz;

namespace {

const std::string kFixtures = ERGO_PACKAGE_MODEL_FIXTURES;

package_model::PackageModel load_host() {
    package_model::LoadOptions o;
    o.build_dir         = kFixtures + "/fileapi_host";
    o.layers_path       = kFixtures + "/layers.json";
    o.require_codemodel = true;
    package_model::PackageModel m;
    package_model::load_package_model(o, m);
    return m;
}

const graph_layout::LayoutNode* find_node(const graph_layout::LayoutInput& g, const std::string& id) {
    for (const auto& n : g.nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

bool has_edge(const graph_layout::LayoutInput& g, const std::string& from, const std::string& to,
              bool dashed) {
    for (const auto& e : g.edges) {
        if (e.from == from && e.to == to && e.dashed == dashed) return true;
    }
    return false;
}

/// n ノードの鎖 (+ 飛ばし辺) の抽象グラフ。 グループは 2 つ。
ViewGraph chain(std::size_t n, std::size_t skip_edges = 0) {
    ViewGraph v;
    v.graph.project = "Ergo";
    v.graph.title   = "chain";
    v.graph.groups  = {{"a", "A", {1, 2, 3}}, {"b", "B", {4, 5, 6}}};
    for (std::size_t i = 0; i < n; ++i) {
        const std::string id = "n" + std::to_string(1000 + i);
        v.graph.nodes.push_back({id, i % 2 ? "b" : "a", id});
        v.info[id] = {"pkg", "", id};
        if (i > 0) v.graph.edges.push_back({"n" + std::to_string(999 + i), id, false});
    }
    for (std::size_t k = 0; k < skip_edges; ++k) {
        const std::size_t a = k % n, b = (k * 7 + 3) % n;
        if (a != b) v.graph.edges.push_back({"n" + std::to_string(1000 + a), "n" + std::to_string(1000 + b), true});
    }
    return v;
}

} // namespace

TEST(PackageViz, PackagesViewNodesLabelsAndEdges) {
    const auto m = load_host();
    ASSERT_TRUE(m.has_codemodel);
    const auto v = build_packages_view(m);
    const auto& g = v.graph;
    EXPECT_EQ(g.nodes.size(), 8u);   // clio curare ergo glfw host lapilli pictor tela
    const auto* pictor = find_node(g, "pictor");
    ASSERT_TRUE(pictor != nullptr);
    EXPECT_EQ(pictor->group_id, std::string(kGroupRender));
    EXPECT_TRUE(pictor->label.find("pictor (Pc)") == 0);
    EXPECT_TRUE(pictor->label.find("版 2.2.0 [cmake]") != std::string::npos);
    EXPECT_TRUE(pictor->label.find("SHA 9b1c2d3") != std::string::npos);
    EXPECT_TRUE(pictor->label.find("★drift:ahead") != std::string::npos);
    EXPECT_TRUE(pictor->label.find("dirty") != std::string::npos);
    EXPECT_TRUE(pictor->label.find("機能 vulkan") != std::string::npos);
    EXPECT_TRUE(find_node(g, "ergo")->label.find("宣言漏れ") != std::string::npos);
    EXPECT_TRUE(find_node(g, "clio")->label.find("未観測") != std::string::npos);
    EXPECT_EQ(find_node(g, "tela")->group_id, std::string(kGroupViz));
    EXPECT_EQ(find_node(g, "glfw")->group_id, std::string(kGroupThirdParty));
    EXPECT_EQ(find_node(g, "clio")->group_id, std::string(kGroupMusa));
    EXPECT_EQ(find_node(g, "curare")->group_id, std::string(kGroupRelated));

    EXPECT_TRUE(has_edge(g, "tela", "pictor", false));    // requires
    EXPECT_TRUE(has_edge(g, "host", "ergo", false));      // 実リンクを畳んだもの
    EXPECT_TRUE(has_edge(g, "ergo", "pictor", false));
    EXPECT_TRUE(has_edge(g, "pictor", "glfw", false));
    EXPECT_TRUE(has_edge(g, "ergo", "clio", true));       // ported_from
    EXPECT_TRUE(has_edge(g, "ergo", "curare", true));     // shares-vocabulary
    EXPECT_EQ(g.edges.size(), 7u);
}

TEST(PackageViz, PackagesViewFitsAndValidates) {
    const auto m = load_host();
    FitResult r;
    std::string error;
    ASSERT_TRUE(make_view(m, {}, r, &error));
    EXPECT_FALSE(r.split);
    ASSERT_EQ(r.graphs.size(), 1u);
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.graphs[0], &error));
    EXPECT_TRUE(r.steps.empty());   // 収まっているので何もかけない
}

TEST(PackageViz, TargetsViewGroupsByLayer) {
    const auto m = load_host();
    const auto v = build_targets_view(m);
    const auto& g = v.graph;
    EXPECT_EQ(g.nodes.size(), 8u);   // テスト・ダミー・gtest_main を除く
    EXPECT_TRUE(find_node(g, "test_render") == nullptr);
    EXPECT_EQ(find_node(g, "ergo_render")->group_id, std::string(kLayerPresentation));
    EXPECT_EQ(find_node(g, "ergo_input")->group_id, std::string(kLayerInfrastructure));
    EXPECT_EQ(find_node(g, "demo_app")->group_id, std::string(kLayerApplication));
    EXPECT_EQ(find_node(g, "pictor")->group_id, std::string(kGroupExternal));
    EXPECT_EQ(find_node(g, "glfw")->group_id, std::string(kGroupThirdParty));
    EXPECT_TRUE(find_node(g, "ergo_gpu_particle")->label.find("★宣言漏れ") != std::string::npos);
    EXPECT_EQ(g.edges.size(), 9u);

    ViewOptions with_tests;
    with_tests.include_tests = true;
    EXPECT_EQ(build_targets_view(m, with_tests).graph.nodes.size(), 11u);
}

TEST(PackageViz, CyclesMoveToCycleGroup) {
    EXPECT_EQ(cyclic_components({{1}, {2}, {0}, {}}).size(), 1u);
    EXPECT_EQ(cyclic_components({{1}, {}, {}}).size(), 0u);
    const auto comps = cyclic_components({{1}, {0}, {3}, {2}, {0}});
    ASSERT_EQ(comps.size(), 2u);
    EXPECT_EQ(comps[0][0], 0u);
    EXPECT_EQ(comps[1][0], 2u);

    auto m = load_host();
    for (auto& t : m.targets) {   // ergo_common → ergo_input で ergo_input ↔ ergo_common
        if (t.name == "ergo_common") {
            for (const auto& u : m.targets) {
                if (u.name == "ergo_input") t.dependencies.push_back(u.id);
            }
        }
    }
    const auto v = build_targets_view(m);
    EXPECT_EQ(find_node(v.graph, "ergo_input")->group_id, std::string(kGroupCycle));
    EXPECT_EQ(find_node(v.graph, "ergo_common")->group_id, std::string(kGroupCycle));
    EXPECT_EQ(v.info.at("ergo_input").layer, std::string(kLayerInfrastructure));
    EXPECT_TRUE(v.graph.title.find("循環 1 組") != std::string::npos);
    FitResult r;
    std::string error;
    ViewOptions o;
    o.view = View::Targets;
    ASSERT_TRUE(make_view(m, o, r, &error));
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.graphs[0], &error));
}

TEST(PackageViz, TransitiveReductionKeepsReachability) {
    graph_layout::LayoutInput g;
    g.edges = {{"a", "b", false}, {"b", "c", false}, {"a", "c", false},
               {"a", "c", true}, {"a", "b", false}};
    EXPECT_EQ(transitive_reduction(g), 2u);   // 重複 1 + a → c
    EXPECT_EQ(g.edges.size(), 3u);
    EXPECT_TRUE(has_edge(g, "a", "b", false));
    EXPECT_TRUE(has_edge(g, "b", "c", false));
    EXPECT_TRUE(has_edge(g, "a", "c", true));   // 破線は残す

    // 循環でも決定的で、 到達関係を壊さない (a → b → a は消さない)。
    graph_layout::LayoutInput c;
    c.edges = {{"a", "b", false}, {"b", "a", false}, {"a", "c", false}, {"b", "c", false}};
    EXPECT_EQ(transitive_reduction(c), 1u);
    EXPECT_TRUE(has_edge(c, "a", "b", false));
    EXPECT_TRUE(has_edge(c, "b", "a", false));
}

TEST(PackageViz, CollapseFoldsOtherPackages) {
    const auto m = load_host();
    auto v = build_targets_view(m);
    const auto collapsed = collapse_packages(v, m, expanded_packages(m, {}));
    ASSERT_EQ(collapsed.size(), 2u);   // glfw(1), pictor(1)
    EXPECT_TRUE(find_node(v.graph, "pkg:pictor") != nullptr);
    EXPECT_TRUE(find_node(v.graph, "pictor") == nullptr);
    EXPECT_EQ(find_node(v.graph, "pkg:glfw")->group_id, std::string(kGroupThirdParty));
    EXPECT_TRUE(has_edge(v.graph, "ergo_render", "pkg:pictor", false));
    EXPECT_TRUE(has_edge(v.graph, "pkg:pictor", "pkg:glfw", false));
    EXPECT_TRUE(find_node(v.graph, "pkg:pictor")->label.find("畳み込み") != std::string::npos);

    // --expand pictor なら pictor は畳まない。
    auto v2 = build_targets_view(m);
    collapse_packages(v2, m, expanded_packages(m, {"pictor"}));
    EXPECT_TRUE(find_node(v2.graph, "pictor") != nullptr);
    EXPECT_TRUE(find_node(v2.graph, "pkg:glfw") != nullptr);
}

TEST(PackageViz, CollapseMergesManyTargetsIntoOneNode) {
    ViewGraph v;
    v.graph.groups = target_groups();
    for (const char* id : {"app", "x1", "x2", "x3"}) {
        v.graph.nodes.push_back({id, kGroupExternal, id});
        v.info[id] = {std::string(id) == "app" ? "host" : "ext", "", id};
    }
    v.graph.edges = {{"app", "x1", false}, {"app", "x2", false}, {"x1", "x2", false}, {"x2", "x3", true}};
    package_model::PackageModel m;
    const auto collapsed = collapse_packages(v, m, {"host"});
    ASSERT_EQ(collapsed.size(), 1u);
    EXPECT_EQ(collapsed[0], std::string("ext(3)"));
    EXPECT_EQ(v.graph.nodes.size(), 2u);
    ASSERT_EQ(v.graph.edges.size(), 1u);   // 自己ループは落とし、 重なりは 1 本
    EXPECT_TRUE(has_edge(v.graph, "app", "pkg:ext", false));
}

TEST(PackageViz, FocusKeepsMatchesAndNeighbours) {
    const auto m = load_host();
    ViewOptions o;
    o.view  = View::Targets;
    o.focus = {"infrastructure"};
    FitResult r;
    std::string error;
    ASSERT_TRUE(make_view(m, o, r, &error));
    std::set<std::string> ids;
    for (const auto& n : r.graphs[0].nodes) ids.insert(n.id);
    // ergo_input と隣 (demo_app, ergo_common)。 pictor は畳まれて隣でなければ消える。
    EXPECT_TRUE(ids.count("ergo_input") == 1);
    EXPECT_TRUE(ids.count("demo_app") == 1);
    EXPECT_TRUE(ids.count("ergo_common") == 1);
    EXPECT_EQ(ids.size(), 3u);
    EXPECT_TRUE(r.graphs[0].title.find("焦点 infrastructure") != std::string::npos);

    o.focus = {"no-such-thing"};
    EXPECT_FALSE(make_view(m, o, r, &error));
    EXPECT_TRUE(error.find("focus matched nothing") != std::string::npos);
}

TEST(PackageViz, LimitExactlyFitsOneSheet) {
    package_model::PackageModel m;
    FitResult r;
    std::string error;
    ViewOptions o;
    ASSERT_TRUE(fit_view(chain(128), m, o, r, &error));
    EXPECT_FALSE(r.split);
    ASSERT_EQ(r.graphs.size(), 1u);
    EXPECT_EQ(r.graphs[0].nodes.size(), 128u);
    const auto files = output_files(r, "out/targets.tela");
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].first, std::string("out/targets.tela"));
}

TEST(PackageViz, OverLimitSplitsWithIndex) {
    package_model::PackageModel m;
    FitResult r;
    std::string error;
    ViewOptions o;
    ASSERT_TRUE(fit_view(chain(129), m, o, r, &error));
    EXPECT_TRUE(r.split);
    ASSERT_EQ(r.graphs.size(), 2u);
    std::size_t total = 0;
    for (const auto& g : r.graphs) {
        EXPECT_TRUE(g.nodes.size() <= tela_graph::kTelaMaxNodes);
        EXPECT_TRUE(tela_graph::validate_tela_graph(g, &error));
        total += g.nodes.size();
    }
    EXPECT_EQ(total, 129u);
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.index, &error));
    EXPECT_EQ(r.index.nodes.size(), 2u);
    EXPECT_TRUE(r.index.title.find("索引") != std::string::npos);
    const auto files = output_files(r, "out/targets.tela");
    ASSERT_EQ(files.size(), 3u);
    EXPECT_EQ(files[0].first, std::string("out/targets.1.tela"));
    EXPECT_EQ(files[1].first, std::string("out/targets.2.tela"));
    EXPECT_EQ(files[2].first, std::string("out/targets.index.tela"));
}

TEST(PackageViz, TooManyEdgesAreReducedThenSplit) {
    // 100 ノードに破線 400 本 (簡約で減らない) → 1 枚 256 辺を超えるので分割。
    package_model::PackageModel m;
    FitResult r;
    std::string error;
    ViewOptions o;
    ASSERT_TRUE(fit_view(chain(100, 400), m, o, r, &error));
    EXPECT_TRUE(r.split);
    for (const auto& g : r.graphs) {
        EXPECT_TRUE(g.edges.size() <= tela_graph::kTelaMaxEdges);
        EXPECT_TRUE(tela_graph::validate_tela_graph(g, &error));
    }
    EXPECT_TRUE(r.steps.front() == "reduce");
}

TEST(PackageViz, SplitPartsRespectNodeLimit) {
    const auto v = chain(10);
    Limits l;
    const auto s = split_graph(v.graph, l, 4);
    ASSERT_EQ(s.parts.size(), 3u);
    EXPECT_EQ(s.parts[0].nodes.size(), 4u);
    EXPECT_EQ(s.parts[2].nodes.size(), 2u);
    std::size_t inner = 0;
    for (const auto& p : s.parts) inner += p.edges.size();
    EXPECT_EQ(inner + s.cut_edges, v.graph.edges.size());
}

TEST(PackageViz, DeterministicOutput) {
    const auto m = load_host();
    ViewOptions o;
    o.view = View::Targets;
    FitResult a, b;
    ASSERT_TRUE(make_view(m, o, a));
    ASSERT_TRUE(make_view(m, o, b));
    EXPECT_EQ(tela_graph::to_tela_graph_text(a.graphs[0]), tela_graph::to_tela_graph_text(b.graphs[0]));
}

TEST(PackageViz, TargetsViewNeedsCodemodel) {
    package_model::PackageModel m;
    ViewOptions o;
    o.view = View::Targets;
    FitResult r;
    std::string error;
    EXPECT_FALSE(make_view(m, o, r, &error));
    EXPECT_TRUE(error.find("File API") != std::string::npos);
}
