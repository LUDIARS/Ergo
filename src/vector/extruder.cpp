#include "ergo/vector/vector.h"

#include <array>
#include <cmath>
#include <vector>

namespace ergo::vector {

void extrude_mesh(VectorMesh& mesh,
                  const std::vector<std::vector<std::array<double, 2>>>& rings,
                  float depth, bool front, bool back, bool walls) {
  if (depth <= 0.0f || mesh.vertices.empty()) return;
  const float hz = depth * 0.5f;
  const size_t base_count = mesh.vertices.size();
  if (front) for (auto& v : mesh.vertices) { v.pos.z = hz; v.normal = {0, 0, 1}; }
  if (back) {
    for (size_t i = 0; i < base_count; ++i) {
      auto b = mesh.vertices[i];
      b.pos.z = -hz; b.normal = {0, 0, -1};
      mesh.vertices.push_back(b);
    }
    const size_t tri_count = mesh.indices.size();
    for (size_t i = 0; i + 2 < tri_count; i += 3) {
      uint32_t a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
      mesh.indices.push_back(static_cast<uint32_t>(base_count) + c);
      mesh.indices.push_back(static_cast<uint32_t>(base_count) + b);
      mesh.indices.push_back(static_cast<uint32_t>(base_count) + a);
    }
  }
  if (!walls) return;
  if (rings.empty()) return;

  for (const auto& ring : rings) {
    if (ring.size() < 2) continue;
    for (size_t i = 0; i < ring.size(); ++i) {
      const auto& a = ring[i];
      const auto& b = ring[(i + 1) % ring.size()];
      const float ax = static_cast<float>(a[0]), ay = static_cast<float>(a[1]);
      const float bx = static_cast<float>(b[0]), by = static_cast<float>(b[1]);
      const float ex = bx - ax, ey = by - ay;
      const float len = std::sqrt(ex * ex + ey * ey);
      if (len <= 1e-6f) continue;
      const float nx = ey / len, ny = -ex / len;
      const uint32_t s = static_cast<uint32_t>(mesh.vertices.size());
      Vertex v0{}; v0.pos={ax,ay,+hz}; v0.normal={nx,ny,0.0f}; v0.uv={0,0}; v0.color={};
      Vertex v1{}; v1.pos={bx,by,+hz}; v1.normal={nx,ny,0.0f}; v1.uv={1,0}; v1.color={};
      Vertex v2{}; v2.pos={bx,by,-hz}; v2.normal={nx,ny,0.0f}; v2.uv={1,1}; v2.color={};
      Vertex v3{}; v3.pos={ax,ay,-hz}; v3.normal={nx,ny,0.0f}; v3.uv={0,1}; v3.color={};
      mesh.vertices.push_back(v0); mesh.vertices.push_back(v1); mesh.vertices.push_back(v2); mesh.vertices.push_back(v3);
      mesh.indices.push_back(s + 0); mesh.indices.push_back(s + 1); mesh.indices.push_back(s + 2);
      mesh.indices.push_back(s + 0); mesh.indices.push_back(s + 2); mesh.indices.push_back(s + 3);
    }
  }
}

} // namespace ergo::vector
