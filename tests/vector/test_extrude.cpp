#include "gtest/gtest.h"
#include "ergo/vector/vector.h"

using namespace ergo::vector;

TEST(VectorExtrude, AddsBackCapVerticesWhenDepthPositive) {
  VectorPath p;
  p.points = {
      {PathCmd::MoveTo, {0, 0}, {}, {}},
      {PathCmd::LineTo, {2, 0}, {}, {}},
      {PathCmd::LineTo, {0, 2}, {}, {}},
      {PathCmd::Close, {}, {}, {}}
  };
  auto flat = build_mesh({p}, TessOptions{}, ExtrudeOptions{0.0f, true, false, false});
  auto ext = build_mesh({p}, TessOptions{}, ExtrudeOptions{1.0f, true, true, false});
  EXPECT_GT(ext.vertices.size(), flat.vertices.size());
  EXPECT_GT(ext.indices.size(), flat.indices.size());
}

