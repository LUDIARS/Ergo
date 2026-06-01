#include "gtest/gtest.h"
#include "ergo/vector/vector.h"

using namespace ergo::vector;

TEST(VectorMesh, BuildsTriangleMeshFromPolygon) {
  VectorPath p;
  p.points = {
      {PathCmd::MoveTo, {0, 0}, {}, {}},
      {PathCmd::LineTo, {1, 0}, {}, {}},
      {PathCmd::LineTo, {0, 1}, {}, {}},
      {PathCmd::Close, {}, {}, {}}
  };
  auto m = build_mesh({p}, TessOptions{}, ExtrudeOptions{});
  EXPECT_GE(m.vertices.size(), 3u);
  EXPECT_GE(m.indices.size(), 3u);
}

