#include "ergo/vector/vector.h"

#include <cmath>

namespace ergo::vector {
namespace {
Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
float dist(const Vec2& a, const Vec2& b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}
int seg_count(float approx_len, float tol, int min_seg, int max_seg) {
  if (tol <= 1e-4f) return max_seg;
  int s = static_cast<int>(std::ceil(approx_len / tol));
  if (s < min_seg) s = min_seg;
  if (s > max_seg) s = max_seg;
  return s;
}
}

std::vector<VectorPath> flatten_paths(const std::vector<VectorPath>& in, [[maybe_unused]] float tol) {
  std::vector<VectorPath> out;
  for (const auto& p : in) {
    VectorPath fp = p;
    fp.points.clear();
    for (const auto& c : p.points) {
      if (c.command == PathCmd::QuadTo) {
        Vec2 start = fp.points.empty() ? Vec2{} : fp.points.back().p;
        const int seg = seg_count(dist(start, c.c1) + dist(c.c1, c.p), tol, 2, 128);
        for (int i = 1; i <= seg; ++i) {
          float t = i / static_cast<float>(seg);
          Vec2 a = lerp(start, c.c1, t);
          Vec2 b = lerp(c.c1, c.p, t);
          Vec2 q = lerp(a, b, t);
          PathPoint l{}; l.command = PathCmd::LineTo; l.p = q; fp.points.push_back(l);
        }
      } else if (c.command == PathCmd::CubicTo) {
        Vec2 start = fp.points.empty() ? Vec2{} : fp.points.back().p;
        const int seg = seg_count(dist(start, c.c1) + dist(c.c1, c.c2) + dist(c.c2, c.p), tol, 3, 192);
        for (int i = 1; i <= seg; ++i) {
          float t = i / static_cast<float>(seg);
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
