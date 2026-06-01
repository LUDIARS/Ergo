#include "gtest/gtest.h"
#include "ergo/vector/vector.h"
#include <cmath>

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

static float tri_area2d(const Vertex& a, const Vertex& b, const Vertex& c) {
  return std::abs((b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x)) * 0.5f;
}

TEST(VectorMesh, SupportsHoleRingsInEarcut) {
  VectorPath p;
  p.points = {
      {PathCmd::MoveTo, {0, 0}, {}, {}}, {PathCmd::LineTo, {4, 0}, {}, {}}, {PathCmd::LineTo, {4, 4}, {}, {}}, {PathCmd::LineTo, {0, 4}, {}, {}}, {PathCmd::Close, {}, {}, {}},
      {PathCmd::MoveTo, {1, 1}, {}, {}}, {PathCmd::LineTo, {1, 3}, {}, {}}, {PathCmd::LineTo, {3, 3}, {}, {}}, {PathCmd::LineTo, {3, 1}, {}, {}}, {PathCmd::Close, {}, {}, {}}
  };
  auto m = build_mesh({p}, TessOptions{}, ExtrudeOptions{});
  float area = 0.0f;
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
    const auto& a = m.vertices[m.indices[i]];
    const auto& b = m.vertices[m.indices[i + 1]];
    const auto& c = m.vertices[m.indices[i + 2]];
    area += tri_area2d(a, b, c);
  }
  // outer 16 - inner 4 = 12
  EXPECT_TRUE(std::abs(area - 12.0f) < 0.05f);
}

TEST(VectorMesh, FlattenTolIncreasesVertexDensity) {
  VectorPath p;
  p.points = {
      {PathCmd::MoveTo, {0, 0}, {}, {}},
      {PathCmd::CubicTo, {3, 0}, {1, 2}, {2, -2}},
      {PathCmd::LineTo, {0, 0}, {}, {}},
      {PathCmd::Close, {}, {}, {}}
  };
  auto coarse = build_mesh({p}, TessOptions{1.0f, FillRule::NonZero}, ExtrudeOptions{});
  auto fine = build_mesh({p}, TessOptions{0.1f, FillRule::NonZero}, ExtrudeOptions{});
  EXPECT_GT(fine.vertices.size(), coarse.vertices.size());
}
