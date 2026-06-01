#include "gtest/gtest.h"
#include "ergo/vector/vector.h"
#include <cmath>

using namespace ergo::vector;

TEST(VectorMorph, WeightUpdatesGeometry) {
  auto s = std::make_unique<VectorScene>();
  std::vector<VectorPath> base(1);
  base[0].points = {
      {PathCmd::MoveTo, {0, 0}, {}, {}},
      {PathCmd::LineTo, {1, 0}, {}, {}},
      {PathCmd::LineTo, {0, 1}, {}, {}},
      {PathCmd::Close, {}, {}, {}}
  };
  s->add_path_node("a", base, ExtrudeOptions{});
  ASSERT_TRUE(s);
  std::vector<VectorPath> target(1);
  target[0].points = {
      {PathCmd::MoveTo, {0, 0}, {}, {}},
      {PathCmd::LineTo, {2, 0}, {}, {}},
      {PathCmd::LineTo, {0, 2}, {}, {}},
      {PathCmd::Close, {}, {}, {}}
  };
  s->add_morph_target("a", target);
  s->set_morph_weight("a", 0.5f);
  s->update(0.016f);
  std::vector<VectorScene::DrawItem> out;
  s->collect(out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_GT(out[0].mesh->vertices.size(), 0u);
  float max_x = -999.0f;
  for (const auto& v : out[0].mesh->vertices) if (v.pos.x > max_x) max_x = v.pos.x;
  EXPECT_TRUE(std::abs(max_x - 1.5f) < 0.001f);
}
