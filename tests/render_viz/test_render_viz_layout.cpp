#include "gtest/gtest.h"

#include "ergo/render_viz/pipeline_layout.h"
#include "ergo/tela_graph/writer.h"

#include <string>

using namespace ergo::render_viz;
namespace tela_graph = ergo::tela_graph;
using ergo::tela_graph::to_tela_graph_text;
using ergo::tela_graph::validate_tela_graph;

namespace {

/// Pictor の High 相当を縮めた構成: 影 → 深度 → 不透明 → ポストプロセス。
PipelineSnapshot sample() {
    PipelineSnapshot s;
    s.profile_name   = "High";
    s.rendering_path = "Forward+";
    s.attachments = {
        {"shadow_map", "D32_SFLOAT", false, true},
        {"hdr_color", "R16G16B16A16_SFLOAT", false, false},
        {"depth", "D32_SFLOAT", false, true},
        {"swapchain", "", true, false},
    };
    PassInfo shadow{"Shadow", PassKind::Shadow, {"shadow_map"}, {}, false, false, 0.5};
    PassInfo depth{"DepthPre", PassKind::DepthOnly, {"depth"}, {}, false, false, 0.25};
    PassInfo opaque{"Opaque", PassKind::Opaque, {"hdr_color", "depth"}, {"shadow_map"}, false, false, 2.0};
    PassInfo post{"Post", PassKind::PostProcess, {"swapchain"}, {"hdr_color", "depth"}, true, false, 1.0};
    s.passes  = {shadow, depth, opaque, post};
    s.effects = {{"Bloom", true}, {"Tonemapping", true}, {"FilmGrain", false}};
    return s;
}

const tela_graph::Node* node(const tela_graph::Graph& g, const std::string& id) {
    for (const auto& n : g.nodes) if (n.id == id) return &n;
    return nullptr;
}

const tela_graph::Edge* edge(const tela_graph::Graph& g, const std::string& from, const std::string& to) {
    for (const auto& e : g.edges) if (e.from == from && e.to == to) return &e;
    return nullptr;
}

bool has_group(const tela_graph::Graph& g, const std::string& id) {
    for (const auto& gr : g.groups) if (gr.id == id) return true;
    return false;
}

} // namespace

TEST(PipelineLayout, PassesRunLeftToRightInExecutionOrder) {
    const auto g = layout_pipeline(sample());
    const auto* a = node(g, "pass:Shadow");
    const auto* b = node(g, "pass:DepthPre");
    const auto* c = node(g, "pass:Opaque");
    const auto* d = node(g, "pass:Post");
    ASSERT_TRUE(a && b && c && d);
    EXPECT_LT(a->x, b->x);
    EXPECT_LT(b->x, c->x);
    EXPECT_LT(c->x, d->x);
    EXPECT_FLOAT_EQ(a->y, d->y);
}

TEST(PipelineLayout, ConsecutivePassesAreLinkedInExecutionOrder) {
    const auto g = layout_pipeline(sample());
    const auto* a = edge(g, "pass:Shadow", "pass:DepthPre");
    const auto* b = edge(g, "pass:DepthPre", "pass:Opaque");
    const auto* c = edge(g, "pass:Opaque", "pass:Post");
    ASSERT_TRUE(a && b && c);
    EXPECT_FALSE(a->dashed);
    EXPECT_FLOAT_EQ(a->points.front().x, node(g, "pass:Shadow")->x + node(g, "pass:Shadow")->width);
    EXPECT_FLOAT_EQ(a->points.back().x, node(g, "pass:DepthPre")->x);
    EXPECT_TRUE(edge(g, "pass:Post", "pass:Shadow") == nullptr);
}

TEST(PipelineLayout, AttachmentRowSitsAbovePassRowAndEffectsBelow) {
    const auto g = layout_pipeline(sample());
    const auto* att  = node(g, "att:hdr_color");
    const auto* pass = node(g, "pass:Opaque");
    const auto* fx   = node(g, "fx:Bloom");
    ASSERT_TRUE(att && pass && fx);
    EXPECT_LT(att->y + att->height, pass->y);
    EXPECT_LT(pass->y + pass->height, fx->y);
}

TEST(PipelineLayout, WritesAreSolidAndReadsAreDashed) {
    const auto g = layout_pipeline(sample());
    const auto* write = edge(g, "pass:Opaque", "att:hdr_color");
    const auto* read  = edge(g, "att:hdr_color", "pass:Post");
    ASSERT_TRUE(write != nullptr);
    ASSERT_TRUE(read != nullptr);
    EXPECT_FALSE(write->dashed);
    EXPECT_TRUE(read->dashed);
    // 書き込みの矢じりはアタッチメントの下辺に、 読み取りはパスの上辺に着く。
    const auto* att  = node(g, "att:hdr_color");
    const auto* post = node(g, "pass:Post");
    EXPECT_FLOAT_EQ(write->points.back().y, att->y + att->height);
    EXPECT_FLOAT_EQ(read->points.back().y, post->y);
}

TEST(PipelineLayout, RoutesAreAxisAlignedPolylines) {
    const auto g = layout_pipeline(sample());
    for (const auto& e : g.edges) {
        ASSERT_TRUE(e.points.size() >= 2u);
        for (std::size_t i = 1; i < e.points.size(); ++i) {
            const auto& p = e.points[i - 1];
            const auto& q = e.points[i];
            EXPECT_TRUE(p.x == q.x || p.y == q.y);       // 縦か横だけ
            EXPECT_FALSE(p.x == q.x && p.y == q.y);      // 長さ 0 の区間なし
        }
    }
}

TEST(PipelineLayout, EffectsChainFromFirstPostProcessPass) {
    const auto g = layout_pipeline(sample());
    ASSERT_TRUE(edge(g, "pass:Post", "fx:Bloom") != nullptr);
    ASSERT_TRUE(edge(g, "fx:Bloom", "fx:Tonemapping") != nullptr);
    ASSERT_TRUE(edge(g, "fx:Tonemapping", "fx:FilmGrain") != nullptr);
    const auto* grain = node(g, "fx:FilmGrain");
    ASSERT_TRUE(grain != nullptr);
    EXPECT_NE(grain->label.find("無効"), std::string::npos);
    EXPECT_EQ(grain->group_id, std::string(kGroupPost));
}

TEST(PipelineLayout, UndeclaredAttachmentIsKeptAndMarked) {
    auto s = sample();
    s.passes[2].inputs.push_back("ssao_result");   // 宣言に無い
    const auto g = layout_pipeline(s);
    const auto* n = node(g, "att:ssao_result");
    ASSERT_TRUE(n != nullptr);
    EXPECT_NE(n->label.find("未宣言"), std::string::npos);
    EXPECT_EQ(node(g, "att:hdr_color")->label.find("未宣言"), std::string::npos);
}

TEST(PipelineLayout, UnreferencedDeclaredAttachmentIsStillShown) {
    auto s = sample();
    s.attachments.push_back({"history", "R16G16B16A16_SFLOAT", false, false});
    const auto g = layout_pipeline(s);
    EXPECT_TRUE(node(g, "att:history") != nullptr);
}

TEST(PipelineLayout, HeaviestKindIsMarked) {
    const auto g = layout_pipeline(sample());
    EXPECT_NE(node(g, "pass:Opaque")->label.find("★最重"), std::string::npos);
    EXPECT_EQ(node(g, "pass:Shadow")->label.find("★最重"), std::string::npos);
    EXPECT_NE(node(g, "pass:Opaque")->label.find("2.00 ms"), std::string::npos);
}

TEST(PipelineLayout, CpuRowAppearsOnlyWithTimings) {
    auto s = sample();
    auto g = layout_pipeline(s);
    EXPECT_FALSE(has_group(g, kGroupCpu));

    s.cpu_stages = {{"DataUpdate", 0.1}, {"Culling", 0.2}, {"Sort", 0.05},
                    {"BatchBuild", 0.1}, {"CommandEncode", 0.3}};
    g = layout_pipeline(s);
    ASSERT_TRUE(has_group(g, kGroupCpu));
    ASSERT_TRUE(edge(g, "cpu:DataUpdate", "cpu:Culling") != nullptr);
    const auto* handoff = edge(g, "cpu:CommandEncode", "pass:Shadow");
    ASSERT_TRUE(handoff != nullptr);
    // 左の通路を通るので、 どのアタッチメント・パスよりも左を降りる。
    const auto* first_att = node(g, "att:shadow_map");
    EXPECT_LT(handoff->points[2].x, first_att->x);
    EXPECT_FLOAT_EQ(handoff->points.back().x, node(g, "pass:Shadow")->x);
}

TEST(PipelineLayout, OnlyGroupsWithNodesAreEmitted) {
    const auto g = layout_pipeline(sample());
    EXPECT_TRUE(has_group(g, kGroupGeometry));
    EXPECT_TRUE(has_group(g, kGroupShading));
    EXPECT_TRUE(has_group(g, kGroupPost));
    EXPECT_TRUE(has_group(g, kGroupAttachment));
    EXPECT_FALSE(has_group(g, kGroupCompute));
}

TEST(PipelineLayout, DuplicatePassNamesGetDistinctIds) {
    auto s = sample();
    s.passes.push_back(s.passes[2]);   // 同名 "Opaque" をもう 1 本
    const auto g = layout_pipeline(s);
    EXPECT_TRUE(node(g, "pass:Opaque") != nullptr);
    EXPECT_TRUE(node(g, "pass:Opaque#2") != nullptr);
}

TEST(PipelineLayout, LayoutIsDeterministic) {
    const auto a = to_tela_graph_text(layout_pipeline(sample()));
    const auto b = to_tela_graph_text(layout_pipeline(sample()));
    EXPECT_FALSE(a.empty());
    EXPECT_EQ(a, b);
}

TEST(PipelineLayout, FitsInsideGraphBoundsAndPassesTelaValidation) {
    auto s = sample();
    s.cpu_stages = {{"DataUpdate", 0.1}, {"CommandEncode", 0.3}};
    const auto g = layout_pipeline(s);
    for (const auto& n : g.nodes) {
        EXPECT_LE(n.x + n.width, g.width);
        EXPECT_LE(n.y + n.height, g.height);
    }
    std::string error;
    EXPECT_TRUE(validate_tela_graph(g, &error));
    EXPECT_EQ(error, std::string());
}
