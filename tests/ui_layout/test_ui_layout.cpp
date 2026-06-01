#include "ergo/ui_layout/ui_layout.h"

#include "ergo/io/file.h"

#include <gtest/gtest.h>

using namespace ergo::ui_layout;

namespace {

struct MockAdapter final : RenderAdapter {
    int rect = 0;
    int image = 0;
    int nine = 0;
    int text = 0;
    int vector = 0;
    int clip_push = 0;
    int clip_pop = 0;

    void draw_rect(const Rect&, std::string_view, float) override { ++rect; }
    void draw_nine_slice(const Rect&, const TextureRef&, float) override { ++nine; }
    void draw_image(const TextureRef&, const Rect&, float) override { ++image; }
    void draw_text(std::string_view, const TextStyle&, const Rect&, float) override { ++text; }
    void draw_vector_mesh(const VectorDrawItem&, float) override { ++vector; }
    void push_clip(const Rect&) override { ++clip_push; }
    void pop_clip() override { ++clip_pop; }
};

std::string sample_json() {
    std::string out;
    if (!ergo::io::read_file("tests/ui_layout/data/sample.uilayout.json", out)) {
        if (!ergo::io::read_file("../tests/ui_layout/data/sample.uilayout.json", out)) {
            return R"JSON({
  "schema_version": 1,
  "name": "sample_hud",
  "design_size": { "w": 1280, "h": 720 },
  "root": {
    "id": "root",
    "type": "container",
    "layout": "absolute",
    "rect": { "x": 0, "y": 0, "w": 1280, "h": 720 },
    "children": [
      {
        "id": "hp_bar",
        "type": "vector",
        "rect": { "x": 24, "y": 56, "w": 320, "h": 28 },
        "vector": { "src": "data/hud/hp_bar.svg", "fit": "stretch", "extrude": 6.0 },
        "binds": [ { "target": "self", "op": "opacity", "expr": "hp_ratio" } ]
      },
      {
        "id": "timer",
        "type": "text",
        "rect": { "x": 540, "y": 16, "w": 200, "h": 32 },
        "text": { "font": "data/fonts/main.ttf", "size": 28, "align": "center", "color": "#ffffff", "value": "00:00" },
        "binds": [ { "target": "self", "op": "text", "expr": "fmt_mmss(time_left)" } ]
      }
    ]
  }
})JSON";
        }
    }
    return out;
}

} // namespace

TEST(UiLayout, LoadFindPatchRoundTrip) {
    auto doc = Document::load_json(sample_json());
    ASSERT_NE(doc, nullptr);
    ASSERT_NE(doc->find("timer"), nullptr);

    doc->apply_patch(R"({"id":"timer","rect":{"x":500}})");
    auto* timer = doc->find("timer");
    ASSERT_NE(timer, nullptr);
    EXPECT_FLOAT_EQ(timer->rect.x, 500.0f);

    const std::string out = doc->to_json();
    auto doc2 = Document::load_json(out);
    ASSERT_NE(doc2, nullptr);
    EXPECT_FLOAT_EQ(doc2->find("timer")->rect.x, 500.0f);
}

TEST(UiLayout, UpdateBindEmitMock) {
    auto doc = Document::load_json(sample_json());
    ASSERT_NE(doc, nullptr);

    BindContext ctx;
    ctx.emplace("time_left", BindValue::from_number(125.0));
    ctx.emplace("hp_ratio", BindValue::from_number(0.75));

    doc->set_viewport(1920, 1080);
    doc->update(ctx, 0.016f);

    auto* timer = doc->find("timer");
    ASSERT_NE(timer, nullptr);
    EXPECT_EQ(timer->text_value, "02:05");

    MockAdapter mock;
    doc->emit(mock);
    EXPECT_EQ(mock.vector, 1);
    EXPECT_EQ(mock.text, 1);
    EXPECT_EQ(mock.clip_push, 1);
    EXPECT_EQ(mock.clip_pop, 1);
}

TEST(UiLayout, HitTestFindsFrontNode) {
    auto doc = Document::load_json(sample_json());
    ASSERT_NE(doc, nullptr);
    doc->update({}, 0.0f);

    Node* node = doc->hit_test(550.0f, 20.0f);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->id, "timer");
}

