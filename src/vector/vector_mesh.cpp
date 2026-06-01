#include "ergo/vector/vector.h"

#include <algorithm>

namespace ergo::vector {
std::vector<VectorPath> flatten_paths(const std::vector<VectorPath>& in, float tol);
void tessellate_path(const VectorPath& path, std::vector<Vec2>& out_v, std::vector<uint32_t>& out_i);
void extrude_mesh(VectorMesh& mesh, float depth, bool front, bool back, bool walls);

VectorMesh build_mesh(const std::vector<VectorPath>& paths, const TessOptions& tess, const ExtrudeOptions& ext) {
  VectorMesh out;
  auto flat = flatten_paths(paths, tess.flatten_tol);
  std::vector<Vec2> poly_v; std::vector<uint32_t> poly_i;
  for (const auto& p : flat) tessellate_path(p, poly_v, poly_i);
  out.vertices.reserve(poly_v.size());
  out.indices = std::move(poly_i);
  for (const auto& p : poly_v) {
    Vertex v{}; v.pos = {p.x, p.y, 0.0f}; v.normal = {0, 0, 1}; v.uv = {p.x, p.y}; v.color = {};
    out.vertices.push_back(v);
  }
  extrude_mesh(out, ext.depth, ext.front, ext.back, ext.walls);
  return out;
}

} // namespace ergo::vector

