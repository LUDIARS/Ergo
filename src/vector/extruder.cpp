#include "ergo/vector/vector.h"

namespace ergo::vector {

void extrude_mesh(VectorMesh& mesh, float depth, bool front, bool back, bool walls) {
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
}

} // namespace ergo::vector

