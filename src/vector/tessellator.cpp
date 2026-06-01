#include "ergo/vector/vector.h"

#include "earcut/earcut.hpp"

#include <array>

namespace ergo::vector {
using Ring = std::vector<std::array<double, 2>>;

std::vector<Ring> contours(const VectorPath& p) {
  std::vector<Ring> out; Ring cur;
  for (const auto& c : p.points) {
    if (c.command == PathCmd::MoveTo) {
      if (!cur.empty()) out.push_back(cur);
      cur.clear(); cur.push_back({c.p.x, c.p.y});
    } else if (c.command == PathCmd::LineTo) {
      cur.push_back({c.p.x, c.p.y});
    } else if (c.command == PathCmd::Close) {
      if (!cur.empty()) { out.push_back(cur); cur.clear(); }
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

void tessellate_path(const VectorPath& path, std::vector<Vec2>& out_v, std::vector<uint32_t>& out_i) {
  auto cs = contours(path);
  if (cs.empty()) return;
  std::vector<Ring> poly = cs;
  auto idx = mapbox::earcut<uint32_t>(poly);
  uint32_t base = static_cast<uint32_t>(out_v.size());
  for (const auto& ring : cs) {
    for (const auto& p : ring) out_v.push_back({static_cast<float>(p[0]), static_cast<float>(p[1])});
  }
  for (uint32_t i : idx) out_i.push_back(base + i);
}

} // namespace ergo::vector
