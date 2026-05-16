#include "ergo/ui_kit/ui_kit.h"

#include <gtest/gtest.h>

using namespace ergo::ui_kit;

namespace {

// 縦 auto-layout の root (1280x720, padding 16, gap 8) に
// 高さ固定の子 2 つ。 横は stretch。
const char* kDoc = R"({
  "root": {
    "name": "Root", "kind": "frame", "w": 1280, "h": 720,
    "layout": { "mode": "vertical", "gap": 8, "pad": [16,16,16,16],
                "align_cross": "stretch" },
    "style": { "fill": [0,0,0,0.5] },
    "children": [
      { "name": "A", "kind": "shape", "w": 100, "h": 40,
        "style": { "fill": [1,0,0,1] } },
      { "name": "B", "kind": "text", "w": 100, "h": 24, "text": "hello",
        "style": { "text_color": [1,1,1,1], "font_size": 14 } }
    ]
  }
})";

} // namespace

TEST(UiKit, ParseBuildsTree) {
    Document doc;
    ASSERT_TRUE(parse_document(kDoc, doc));
    EXPECT_EQ(doc.root.name, "Root");
    EXPECT_EQ(doc.root.kind, NodeKind::Frame);
    EXPECT_EQ(doc.root.layout.mode, LayoutMode::Vertical);
    ASSERT_EQ(doc.root.children.size(), 2u);
    EXPECT_EQ(doc.root.children[0].name, "A");
    EXPECT_EQ(doc.root.children[1].kind, NodeKind::Text);
    EXPECT_EQ(doc.root.children[1].text, "hello");
}

TEST(UiKit, VerticalAutoLayoutPlacesChildren) {
    Document doc;
    ASSERT_TRUE(parse_document(kDoc, doc));
    UIContext ctx;
    ctx.set_document(doc);
    ctx.set_viewport(1280, 720);
    ctx.update(0.0f);

    Node* a = ctx.find("Root/A");
    Node* b = ctx.find("Root/B");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // padding 16 → 最初の子は (16,16)。
    EXPECT_FLOAT_EQ(a->resolved_rect.x, 16.0f);
    EXPECT_FLOAT_EQ(a->resolved_rect.y, 16.0f);
    // align_cross=stretch → 幅は 1280 - 16*2 = 1248。
    EXPECT_FLOAT_EQ(a->resolved_rect.w, 1248.0f);
    EXPECT_FLOAT_EQ(a->resolved_rect.h, 40.0f);
    // 2 番目の子は A の下 + gap 8 → y = 16 + 40 + 8 = 64。
    EXPECT_FLOAT_EQ(b->resolved_rect.y, 64.0f);
    EXPECT_FLOAT_EQ(b->resolved_rect.w, 1248.0f);
}

TEST(UiKit, DrawListEmitsRectAndText) {
    Document doc;
    ASSERT_TRUE(parse_document(kDoc, doc));
    UIContext ctx;
    ctx.set_document(doc);
    ctx.set_viewport(1280, 720);
    ctx.update(0.0f);

    const auto& dl = ctx.draw_list();
    // root fill + A fill + B text = 3 コマンド (B は fill a=0 なので text のみ)。
    int rects = 0, texts = 0;
    for (const auto& d : dl) {
        if (d.kind == DrawKind::Rect) ++rects;
        if (d.kind == DrawKind::Text) ++texts;
    }
    EXPECT_EQ(rects, 2);   // root + A
    EXPECT_EQ(texts, 1);   // B
}

TEST(UiKit, HorizontalCenterAlign) {
    const char* doc_json = R"({
      "root": { "name": "R", "kind": "frame", "w": 200, "h": 100,
        "layout": { "mode": "horizontal", "gap": 0, "align_main": "center" },
        "children": [
          { "name": "C", "kind": "shape", "w": 40, "h": 20 }
        ] } })";
    Document doc;
    ASSERT_TRUE(parse_document(doc_json, doc));
    UIContext ctx;
    ctx.set_document(doc);
    ctx.set_viewport(200, 100);
    ctx.update(0.0f);
    Node* c = ctx.find("R/C");
    ASSERT_NE(c, nullptr);
    // 幅 40 を 200 の中央に → x = (200-40)/2 = 80。
    EXPECT_FLOAT_EQ(c->resolved_rect.x, 80.0f);
}

TEST(UiKit, SerializeRoundTrip) {
    Document doc;
    ASSERT_TRUE(parse_document(kDoc, doc));
    std::string s = serialize_document(doc);
    Document doc2;
    ASSERT_TRUE(parse_document(s, doc2));
    EXPECT_EQ(doc2.root.name, "Root");
    ASSERT_EQ(doc2.root.children.size(), 2u);
    EXPECT_EQ(doc2.root.children[1].text, "hello");
    EXPECT_EQ(doc2.root.layout.mode, LayoutMode::Vertical);
}

TEST(UiKit, OpacityAnimationTweensToTarget) {
    Document doc;
    ASSERT_TRUE(parse_document(kDoc, doc));
    UIContext ctx;
    ctx.set_document(doc);
    ctx.set_viewport(1280, 720);
    ctx.update(0.0f);   // 初期レイアウト (resolved_path 確定)

    ctx.animate("Root/A", AnimProp::Opacity, 0.0f, 1.0f, Easing::Linear);
    Node* a = ctx.find("Root/A");
    ASSERT_NE(a, nullptr);
    EXPECT_FLOAT_EQ(a->opacity, 1.0f);   // 開始値

    ctx.update(0.5f);
    a = ctx.find("Root/A");
    EXPECT_NEAR(a->opacity, 0.5f, 0.01f);  // linear, 半分

    ctx.update(0.6f);                      // t > 1 → 終端でクランプ
    a = ctx.find("Root/A");
    EXPECT_FLOAT_EQ(a->opacity, 0.0f);
}

TEST(UiKit, ClickEventFiresOnNode) {
    Document doc;
    ASSERT_TRUE(parse_document(kDoc, doc));
    UIContext ctx;
    ctx.set_document(doc);
    ctx.set_viewport(1280, 720);

    int clicks = 0;
    ctx.on("Root/A", EventKind::Click, [&](Event&) { ++clicks; });

    // A の矩形は (16,16,1248,40)。 中央 (200,30) でクリック。
    ctx.feed_input({200.0f, 30.0f, false});
    ctx.update(0.0f);   // first_input — 基準を取るだけ
    ctx.feed_input({200.0f, 30.0f, true});
    ctx.update(0.0f);   // PointerDown
    ctx.feed_input({200.0f, 30.0f, false});
    ctx.update(0.0f);   // PointerUp → Click
    EXPECT_EQ(clicks, 1);
}

TEST(UiKit, ClickBubblesToParent) {
    Document doc;
    ASSERT_TRUE(parse_document(kDoc, doc));
    UIContext ctx;
    ctx.set_document(doc);
    ctx.set_viewport(1280, 720);

    int root_clicks = 0;
    ctx.on("Root", EventKind::Click, [&](Event&) { ++root_clicks; });

    ctx.feed_input({200.0f, 30.0f, false});
    ctx.update(0.0f);
    ctx.feed_input({200.0f, 30.0f, true});
    ctx.update(0.0f);
    ctx.feed_input({200.0f, 30.0f, false});
    ctx.update(0.0f);
    // A 上でクリック → Root へバブリング。
    EXPECT_EQ(root_clicks, 1);
}
