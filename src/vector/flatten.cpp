#include "ergo/vector/vector.h"

#include <cmath>

namespace ergo::vector {
namespace {
Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
}

std::vector<VectorPath> flatten_paths(const std::vector<VectorPath>& in, float tol);

std::vector<VectorPath> flatten_paths(const std::vector<VectorPath>& in, float /*tol*/) {
  std::vector<VectorPath> out;
  for (const auto& p : in) {
    VectorPath fp = p;
    fp.points.clear();
    for (const auto& c : p.points) {
      if (c.command == PathCmd::QuadTo) {
        Vec2 start = fp.points.empty() ? Vec2{} : fp.points.back().p;
        for (int i = 1; i <= 8; ++i) {
          float t = i / 8.0f;
          Vec2 a = lerp(start, c.c1, t);
          Vec2 b = lerp(c.c1, c.p, t);
          Vec2 q = lerp(a, b, t);
          PathPoint l{}; l.command = PathCmd::LineTo; l.p = q; fp.points.push_back(l);
        }
      } else if (c.command == PathCmd::CubicTo) {
        Vec2 start = fp.points.empty() ? Vec2{} : fp.points.back().p;
        for (int i = 1; i <= 12; ++i) {
          float t = i / 12.0f;
          Vec2 a = lerp(start, c.c1, t), b = lerp(c.c1, c.c2, t), d = lerp(c.c2, c.p, t);
          Vec2 e = lerp(a, b, t), f = lerp(b, d, t), g = lerp(e, f, t);
          PathPoint l{}; l.command = PathCmd::LineTo; l.p = g; fp.points.push_back(l);
        }
      } else fp.points.push_back(c);
    }
    out.push_back(std::move(fp));
  }
  return out;
}
} // namespace ergo::vector

