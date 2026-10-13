// Tela がある構成だけでビルドされる契約テスト。 Ergo が書いた `TELA_GRAPH 1` を
// Tela 本体の読み込み (tela::load_graph) が受け付け、 プロセス内変換
// (to_tela_graph) と同じ図になることを確かめる。

#include "gtest/gtest.h"

#include "ergo/tela_graph/tela_adapter.h"
#include "ergo/tela_graph/writer.h"

#include <filesystem>
#include <stdexcept>
#include <string>

using namespace ergo::tela_graph;

namespace {

/// 引用符・バックスラッシュ・日本語・小数座標・破線を含む図。
Graph sample() {
    Graph g;
    g.project = "Ergo";
    g.title   = "図 \"quoted\" \\ path";
    g.width   = 640.5f;
    g.height  = 320.25f;
    g.groups  = {{"a", "グループA", true, {66, 133, 244}},
                 {"b", "グループB", false, {245, 166, 35}}};
    g.nodes   = {{"a", "n1", "ノード 1", 32, 32, 200, 72},
                 {"b", "n2", "ノード \"2\"", 280.5f, 32, 200, 72},
                 {"a", "n3", "ノード 3", 32, 192, 200, 72}};
    Edge solid;
    solid.id     = "e0";
    solid.from   = "n1";
    solid.to     = "n2";
    solid.points = {{232, 68}, {280.5f, 68}};
    Edge dashed;
    dashed.id     = "e1";
    dashed.from   = "n2";
    dashed.to     = "n3";
    dashed.dashed = true;
    dashed.points = {{380.5f, 104}, {380.5f, 150.33f}, {132, 150.33f}, {132, 192}};
    g.edges = {solid, dashed};
    return g;
}

} // namespace

TEST(TelaGraphAdapter, TelaLoadsTheWrittenFileAsTheSameGraph) {
    const auto graph = sample();
    const auto dir   = std::filesystem::temp_directory_path() / "ergo_tela_graph_adapter_test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "graph.tela";

    std::string error;
    ASSERT_TRUE(save_tela_graph(path.string(), graph, &error));
    const tela::Graph loaded    = tela::load_graph(path);
    const tela::Graph converted = to_tela_graph(graph);

    EXPECT_TRUE(loaded.info() == converted.info());
    EXPECT_TRUE(loaded.groups() == converted.groups());
    EXPECT_TRUE(loaded.nodes() == converted.nodes());
    ASSERT_EQ(loaded.edges().size(), converted.edges().size());
    for (std::size_t i = 0; i < loaded.edges().size(); ++i) {
        const auto& a = loaded.edges()[i];
        const auto& b = converted.edges()[i];
        EXPECT_TRUE(a.id == b.id && a.from == b.from && a.to == b.to && a.dashed == b.dashed);
        ASSERT_EQ(a.points.size(), b.points.size());
        for (std::size_t k = 0; k < a.points.size(); ++k) {
            // ファイルは小数 2 桁で丸める。
            EXPECT_NEAR(a.points[k].x, b.points[k].x, 0.006f);
            EXPECT_NEAR(a.points[k].y, b.points[k].y, 0.006f);
        }
    }
    std::filesystem::remove_all(dir);
}

TEST(TelaGraphAdapter, InvalidGraphThrowsInsteadOfPartialGraph) {
    auto graph = sample();
    graph.nodes[1].id = graph.nodes[0].id;
    bool threw = false;
    try {
        (void)to_tela_graph(graph);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}
