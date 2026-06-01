#include "gtest/gtest.h"
#include "ergo/vector/vector.h"
#include <cmath>

using namespace ergo::vector;

TEST(VectorScene, CollectReturnsDrawItemsAndDirtyToggles) {
  auto s = VectorScene::load_svg_data("<svg><rect id=\"n\" x=\"0\" y=\"0\" width=\"4\" height=\"3\"/></svg>");
  ASSERT_TRUE(s);
  Transform t{};
  t.translate = {3.0f, 4.0f, 5.0f};
  t.rotate = {0.0f, 0.0f, 0.5f};
  t.scale = {2.0f, 3.0f, 1.0f};
  s->set_node_transform("n", t);
  s->set_scale_x("n", 0.5f);
  s->set_extrude_depth("n", 2.0f);
  s->update(0.016f);
  std::vector<VectorScene::DrawItem> items;
  s->collect(items);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_TRUE(std::abs(items[0].model.m[3] - 3.0f) < 1e-4f);
  EXPECT_TRUE(std::abs(items[0].model.m[7] - 4.0f) < 1e-4f);
  EXPECT_TRUE(std::abs(items[0].model.m[11] - 5.0f) < 1e-4f);
  EXPECT_NE(items[0].model.m[0], 1.0f);
  EXPECT_GT(items[0].mesh->vertices.size(), 0u);
}
