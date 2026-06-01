#include "ergo/vector/vector.h"

#include <array>
#include <algorithm>

namespace ergo::vector {
using Ring = std::vector<std::array<double, 2>>;
std::vector<VectorPath> flatten_paths(const std::vector<VectorPath>& in, float tol);
std::vector<Ring> contours(const VectorPath& p);
void tessellate_path(const VectorPath& path, std::vector<Vec2>& out_v, std::vector<uint32_t>& out_i);
void extrude_mesh(VectorMesh& mesh, const std::vector<Ring>& rings, float depth, bool front, bool back, bool walls);

VectorMesh build_mesh(const std::vector<VectorPath>& paths, const TessOptions& tess, const ExtrudeOptions& ext) {
  VectorMesh out;
  auto flat = flatten_paths(paths, tess.flatten_tol);
  std::vector<Vec2> poly_v; std::vector<uint32_t> poly_i;
  std::vector<Ring> rings;
  for (const auto& p : flat) {
    auto rs = contours(p);
    rings.insert(rings.end(), rs.begin(), rs.end());
  }
  for (const auto& p : flat) tessellate_path(p, poly_v, poly_i);
  out.vertices.reserve(poly_v.size());
  out.indices = std::move(poly_i);
  for (const auto& p : poly_v) {
    Vertex v{}; v.pos = {p.x, p.y, 0.0f}; v.normal = {0, 0, 1}; v.uv = {p.x, p.y}; v.color = {};
    out.vertices.push_back(v);
  }
  extrude_mesh(out, rings, ext.depth, ext.front, ext.back, ext.walls);
  return out;
}

} // namespace ergo::vector
