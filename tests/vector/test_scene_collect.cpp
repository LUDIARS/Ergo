#include "gtest/gtest.h"
#include "ergo/vector/vector.h"

using namespace ergo::vector;

TEST(VectorScene, CollectReturnsDrawItemsAndDirtyToggles) {
  auto s = VectorScene::load_svg_data("<svg><rect id=\"n\" x=\"0\" y=\"0\" width=\"4\" height=\"3\"/></svg>");
  ASSERT_TRUE(s);
  s->set_scale_x("n", 0.5f);
  s->set_extrude_depth("n", 2.0f);
  s->update(0.016f);
  std::vector<VectorScene::DrawItem> items;
  s->collect(items);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_NE(items[0].model.m[0], 1.0f);
  EXPECT_GT(items[0].mesh->vertices.size(), 0u);
}

