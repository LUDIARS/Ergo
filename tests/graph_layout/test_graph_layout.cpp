// ergo_graph_layout の配置の性質: 決定性・左 → 右・最長経路の列・縦横の折れ線・
// 長さ 0 の区間なし・ノードを横切らない・札の行数からの高さ・循環・±100000 内。

#include "gtest/gtest.h"

#include "ergo/graph_layout/layered_layout.h"
#include "ergo/tela_graph/writer.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>

using namespace ergo;
using graph_layout::LayoutInput;
using graph_layout::layout_layered;

namespace {

LayoutInput make(std::initializer_list<const char*> nodes,
                 std::initializer_list<std::pair<const char*, const char*>> edges) {
    LayoutInput in;
    in.project = "Ergo";
    in.title   = "test";
    in.groups  = {{"g", "G", {66, 133, 244}}};
    for (const char* n : nodes) in.nodes.push_back({n, "g", n});
    for (const auto& e : edges) in.edges.push_back({e.first, e.second, false});
    return in;
}

const tela_graph::Node& node(const tela_graph::Graph& g, const std::string& id) {
    for (const auto& n : g.nodes) {
        if (n.id == id) return n;
    }
    static tela_graph::Node none;
    return none;
}

/// 決定的な擬似乱数のグラフ (LCG)。
LayoutInput random_graph(std::size_t n, std::size_t m, std::uint32_t seed) {
    LayoutInput in;
    in.project = "Ergo";
    in.title   = "random";
    in.groups  = {{"a", "A", {1, 2, 3}}, {"b", "B", {4, 5, 6}}};
    for (std::size_t i = 0; i < n; ++i) {
        const std::string id = "n" + std::to_string(i);
        in.nodes.push_back({id, i % 3 == 0 ? "b" : "a", id + (i % 4 == 0 ? " | 2 行目" : "")});
    }
    std::uint32_t s = seed;
    auto next = [&]() { s = s * 1664525u + 1013904223u; return s >> 8; };
    for (std::size_t k = 0; k < m; ++k) {
        const std::size_t a = next() % n, b = next() % n;
        in.edges.push_back({"n" + std::to_string(a), "n" + std::to_string(b), k % 5 == 0});
    }
    return in;
}

/// 線分 (縦か横) がノードの内側を通るか (境界に触れるだけは通らないとみなす)。
bool crosses(const tela_graph::Point& p, const tela_graph::Point& q, const tela_graph::Node& n) {
    const float x0 = std::fmin(p.x, q.x), x1 = std::fmax(p.x, q.x);
    const float y0 = std::fmin(p.y, q.y), y1 = std::fmax(p.y, q.y);
    return x1 > n.x + 0.5f && x0 < n.x + n.width - 0.5f && y1 > n.y + 0.5f &&
           y0 < n.y + n.height - 0.5f;
}

} // namespace

TEST(GraphLayout, LineCountDrivesNodeHeight) {
    graph_layout::LayoutOptions o;
    EXPECT_EQ(graph_layout::label_line_count("a"), 1u);
    EXPECT_EQ(graph_layout::label_line_count("a | b | c"), 3u);
    EXPECT_EQ(graph_layout::label_line_count(""), 1u);
    EXPECT_FLOAT_EQ(graph_layout::node_height_for("a | b | c", o),
                    o.node_padding * 2.0f + o.line_height * 3.0f);

    auto in = make({"x", "y"}, {{"x", "y"}});
    in.nodes[1].label = "y | 2 | 3";
    const auto g = layout_layered(in).graph;
    EXPECT_TRUE(node(g, "y").height > node(g, "x").height);
}

TEST(GraphLayout, UsersLeftOfUsedAndLongestPathColumns) {
    // a → b → c、 a → c: c は最長経路で列 2。
    const auto r = layout_layered(make({"a", "b", "c"}, {{"a", "b"}, {"b", "c"}, {"a", "c"}}));
    EXPECT_EQ(r.columns, 3u);
    const auto& g = r.graph;
    EXPECT_TRUE(node(g, "a").x < node(g, "b").x);
    EXPECT_TRUE(node(g, "b").x < node(g, "c").x);
}

TEST(GraphLayout, DeterministicBytes) {
    const auto in = random_graph(60, 140, 7);
    const auto t1 = tela_graph::to_tela_graph_text(layout_layered(in).graph);
    const auto t2 = tela_graph::to_tela_graph_text(layout_layered(in).graph);
    ASSERT_FALSE(t1.empty());
    EXPECT_EQ(t1, t2);
}

TEST(GraphLayout, OrthogonalRoutesWithoutZeroLengthSegments) {
    const auto r = layout_layered(random_graph(60, 140, 11));
    std::string error;
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.graph, &error));
    for (const auto& e : r.graph.edges) {
        ASSERT_TRUE(e.points.size() >= 2);
        for (std::size_t i = 0; i + 1 < e.points.size(); ++i) {
            const auto& p = e.points[i];
            const auto& q = e.points[i + 1];
            EXPECT_TRUE(p.x == q.x || p.y == q.y);       // 縦か横
            EXPECT_FALSE(p.x == q.x && p.y == q.y);      // 長さ 0 なし
        }
    }
}

TEST(GraphLayout, EndpointsTouchNodeSides) {
    const auto g = layout_layered(make({"a", "b"}, {{"a", "b"}})).graph;
    ASSERT_EQ(g.edges.size(), 1u);
    const auto& e = g.edges[0];
    EXPECT_FLOAT_EQ(e.points.front().x, node(g, "a").x + node(g, "a").width);
    EXPECT_FLOAT_EQ(e.points.back().x, node(g, "b").x);
}

TEST(GraphLayout, LongEdgesUseChannelsAndAvoidNodes) {
    // a → b → c → d と a → d。 a → d は途中の列の b / c を横切らない。
    const auto g = layout_layered(
        make({"a", "b", "c", "d"}, {{"a", "b"}, {"b", "c"}, {"c", "d"}, {"a", "d"}})).graph;
    for (const auto& e : g.edges) {
        for (std::size_t i = 0; i + 1 < e.points.size(); ++i) {
            for (const auto& n : g.nodes) {
                if (n.id == e.from || n.id == e.to) continue;
                EXPECT_FALSE(crosses(e.points[i], e.points[i + 1], n));
            }
        }
    }
}

TEST(GraphLayout, RandomGraphRoutesDoNotCrossNodes) {
    const auto g = layout_layered(random_graph(40, 70, 3)).graph;
    for (const auto& e : g.edges) {
        for (std::size_t i = 0; i + 1 < e.points.size(); ++i) {
            for (const auto& n : g.nodes) {
                if (n.id == e.from || n.id == e.to) continue;
                EXPECT_FALSE(crosses(e.points[i], e.points[i + 1], n));
            }
        }
    }
}

TEST(GraphLayout, CyclesAreDrawnNotDropped) {
    const auto r = layout_layered(make({"a", "b", "c"}, {{"a", "b"}, {"b", "c"}, {"c", "a"}}));
    EXPECT_EQ(r.graph.edges.size(), 3u);
    EXPECT_EQ(r.dropped_edges, 0u);
    std::string error;
    EXPECT_TRUE(tela_graph::validate_tela_graph(r.graph, &error));
    // 戻り辺は向きを保つ (from / to は入力のまま)。
    EXPECT_EQ(r.graph.edges[2].from, std::string("c"));
    EXPECT_EQ(r.graph.edges[2].to, std::string("a"));
}

TEST(GraphLayout, SelfLoopsAndUnknownEndsAreDropped) {
    auto in = make({"a", "b"}, {{"a", "a"}, {"a", "zz"}, {"a", "b"}});
    const auto r = layout_layered(in);
    EXPECT_EQ(r.dropped_edges, 2u);
    ASSERT_EQ(r.graph.edges.size(), 1u);
    EXPECT_EQ(r.graph.edges[0].id, std::string("e0"));
}

TEST(GraphLayout, TiesAreOrderedById) {
    // 辺の無いノードは同じ列 (0) に ID 順で上から並ぶ。
    const auto g = layout_layered(make({"zeta", "alpha", "mid"}, {})).graph;
    EXPECT_TRUE(node(g, "alpha").y < node(g, "mid").y);
    EXPECT_TRUE(node(g, "mid").y < node(g, "zeta").y);
}

TEST(GraphLayout, UnusedGroupsAreOmitted) {
    auto in = make({"a"}, {});
    in.groups.push_back({"unused", "U", {0, 0, 0}});
    const auto g = layout_layered(in).graph;
    ASSERT_EQ(g.groups.size(), 1u);
    EXPECT_EQ(g.groups[0].id, std::string("g"));
}

TEST(GraphLayout, CanvasStaysWithinTelaCoordinateLimit) {
    // 128 ノードの鎖を幅 2000 で並べると 26 万を超える → 等倍に縮めて ±100000 に収める。
    LayoutInput in;
    in.project = "Ergo";
    in.title   = "wide";
    in.groups  = {{"g", "G", {1, 1, 1}}};
    for (int i = 0; i < 128; ++i) {
        in.nodes.push_back({"n" + std::to_string(1000 + i), "g", "x"});
        if (i > 0) in.edges.push_back({"n" + std::to_string(999 + i), "n" + std::to_string(1000 + i), false});
    }
    graph_layout::LayoutOptions o;
    o.node_width = 2000.0f;
    const auto g = layout_layered(in, o).graph;
    std::string error;
    EXPECT_TRUE(tela_graph::validate_tela_graph(g, &error));
    EXPECT_TRUE(g.width <= tela_graph::kTelaMaxCoord);
    for (const auto& n : g.nodes) EXPECT_TRUE(n.x + n.width <= tela_graph::kTelaMaxCoord);
}
