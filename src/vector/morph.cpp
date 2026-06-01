#include "ergo/vector/vector.h"

namespace ergo::vector {
std::vector<VectorPath> morph_blend(const std::vector<VectorPath>& a, const std::vector<VectorPath>& b, float w) {
  if (a.size() != b.size()) return a;
  auto out = a;
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i].points.size() != b[i].points.size()) continue;
    for (size_t j = 0; j < out[i].points.size(); ++j) {
      out[i].points[j].p.x = out[i].points[j].p.x + (b[i].points[j].p.x - out[i].points[j].p.x) * w;
      out[i].points[j].p.y = out[i].points[j].p.y + (b[i].points[j].p.y - out[i].points[j].p.y) * w;
    }
  }
  return out;
}
} // namespace ergo::vector

