#include "scene_preview_raster.h"

#include "ergo/common/json_min.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ergo::studio::app {
namespace {
using Point = std::array<double, 4>;
Point transform(const double* m, const Point& v) {
    Point out{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out[r] += m[c * 4 + r] * v[c];
    return out;
}
double distance_to_plane(const Point& v, int plane) {
    switch (plane) {
        case 0: return v[3] + v[0];
        case 1: return v[3] - v[0];
        case 2: return v[3] + v[1];
        case 3: return v[3] - v[1];
        case 4: return v[2];
        default: return v[3] - v[2];
    }
}
using Polygon = std::array<Point, 12>; // A triangle gains at most one vertex per clipping plane.
int clip(Polygon& polygon, int count, int plane) {
    Polygon next{};
    int n = 0;
    Point previous = polygon[count - 1];
    double a = distance_to_plane(previous, plane);
    for (int i = 0; i < count; ++i) {
        const Point& current = polygon[i];
        const double b = distance_to_plane(current, plane);
        if ((a < 0 && b > 0) || (a > 0 && b < 0)) {
            const double t = a / (a - b);
            for (int k = 0; k < 4; ++k) next[n][k] = previous[k] + t * (current[k] - previous[k]);
            ++n;
        }
        if (b >= 0) next[n++] = current;
        previous = current;
        a = b;
    }
    polygon = next;
    return n;
}
double edge(const Point& a, const Point& b, double x, double y) {
    return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
}
void fill(ScenePreviewRaster& out, std::vector<float>& depth, const Point& a, const Point& b,
          const Point& c, float shade, size_t& budget) {
    const double area = edge(a, b, c[0], c[1]);
    if (std::abs(area) < 1e-9) return;
    const int xmin = std::max(0, int(std::floor(std::min({a[0], b[0], c[0]}))));
    const int xmax = std::min(out.width - 1, int(std::ceil(std::max({a[0], b[0], c[0]}))));
    const int ymin = std::max(0, int(std::floor(std::min({a[1], b[1], c[1]}))));
    const int ymax = std::min(out.height - 1, int(std::ceil(std::max({a[1], b[1], c[1]}))));
    const auto samples =
        size_t(std::max(0, xmax - xmin + 1)) * size_t(std::max(0, ymax - ymin + 1));
    if (samples > budget) {
        out.notice = "編集用メッシュの重なりを減らしてください";
        return;
    }
    budget -= samples;
    for (int y = ymin; y <= ymax; ++y)
        for (int x = xmin; x <= xmax; ++x) {
            const double u = edge(b, c, x + .5, y + .5) / area,
                         v = edge(c, a, x + .5, y + .5) / area, w = 1 - u - v;
            if (std::min({u, v, w}) < 0) continue;
            const float z = static_cast<float>(u * a[2] + v * b[2] + w * c[2]);
            const auto i = static_cast<std::size_t>(y * out.width + x);
            if (z < depth[i]) {
                depth[i] = z;
                out.diffuse[i] = shade;
            }
        }
}
void triangle(ScenePreviewRaster& out, std::vector<float>& depth, const double* matrices,
              const std::array<Point, 3>& vertices, size_t& budget) {
    Polygon polygon{};
    for (int i = 0; i < 3; ++i) {
        polygon[i] = transform(matrices + 16, transform(matrices, vertices[i]));
        polygon[i][0] *= double(out.height) / out.width;
        for (double v : polygon[i])
            if (!std::isfinite(v) || std::abs(v) > 1e30) return;
    }
    int count = 3;
    for (int i = 0; i < 6 && count; ++i) count = clip(polygon, count, i);
    if (count < 3) return;
    for (int i = 0; i < count; ++i) {
        auto& p = polygon[i];
        if (p[3] <= 0) return;
        p[0] = (std::clamp(p[0] / p[3], -1., 1.) + 1) * .5 * out.width;
        p[1] = (std::clamp(p[1] / p[3], -1., 1.) + 1) * .5 * out.height;
        p[2] /= p[3];
    }
    Point u{}, v{};
    for (int k = 0; k < 3; ++k) {
        u[k] = vertices[1][k] - vertices[0][k];
        v[k] = vertices[2][k] - vertices[0][k];
    }
    const double nx = u[1] * v[2] - u[2] * v[1], ny = u[2] * v[0] - u[0] * v[2],
                 nz = u[0] * v[1] - u[1] * v[0];
    const double length = std::hypot(nx, ny, nz);
    if (!(length > 0) || !std::isfinite(length)) return;
    const double orientation =
        edge(polygon[0], polygon[1], polygon[2][0], polygon[2][1]) > 0 ? -1. : 1.;
    const float shade = static_cast<float>(
        std::max(0., orientation * (.4 * nx + .8 * ny + .5 * nz) / (length * std::sqrt(1.05))));
    for (int i = 1; i + 1 < count && out.notice.empty(); ++i)
        fill(out, depth, polygon[0], polygon[i], polygon[i + 1], shade, budget);
}
} // namespace
ScenePreviewRaster raster_scene(const plugins::scene_editor::SceneInfo& scene, int width,
                                int height) {
    ScenePreviewRaster out;
    if (scene.camera_matrices.size() != 32) {
        out.notice = "シーンカメラの行列を待っています";
        return out;
    }
    const double scale = std::min(1., 960. / std::max({1, width, height}));
    out.width = std::max(1, int(width * scale));
    out.height = std::max(1, int(height * scale));
    out.diffuse.assign(out.width * out.height, -1);
    std::vector<float> depth(out.diffuse.size(), std::numeric_limits<float>::infinity());
    std::size_t coordinates = 0, budget = 32000000;
    for (const auto& actor : scene.actors)
        for (const auto& component : actor.components) {
            if (component.type != "EditorPreviewMesh") continue;
            for (const auto& [key, text] : component.props) {
                if (key != "worldTriangles") continue;
                common::jsonm::JsonValue points;
                if (!common::jsonm::parse(text, points) || !points.is_array() ||
                    points.a->size() % 9 || points.a->size() > 900000 - coordinates) {
                    out.notice = "編集用形状の形式または頂点数を確認してください";
                    return out;
                }
                coordinates += points.a->size();
                for (std::size_t j = 0; j < points.a->size(); j += 9) {
                    std::array<Point, 3> vertices{};
                    for (int k = 0; k < 9; ++k) {
                        const auto& value = points.a->at(j + k);
                        if (!value.is_number() || !std::isfinite(value.n)) {
                            out.notice = "編集用形状が不正です";
                            return out;
                        }
                        vertices[k / 3][k % 3] = value.n;
                        vertices[k / 3][3] = 1;
                    }
                    triangle(out, depth, scene.camera_matrices.data(), vertices, budget);
                    if (!out.notice.empty()) return out;
                }
            }
        }
    if (!coordinates) out.notice = "このシーンは編集用形状を配信していません";
    return out;
}
std::shared_ptr<const ::tela::Image> ScenePreviewRaster::image(float intensity) const {
    auto pixels = std::make_shared<::tela::Image>();
    pixels->width = width;
    pixels->height = height;
    pixels->pixels.reserve(diffuse.size() * 4);
    for (float d : diffuse) {
        const float light = .16f + intensity * d;
        auto channel = [&](float base) {
            return static_cast<unsigned char>(std::clamp(base * light, 0.f, 1.f) * 255.f);
        };
        if (d < 0)
            pixels->pixels.insert(pixels->pixels.end(), {38, 31, 23, 255});
        else
            pixels->pixels.insert(pixels->pixels.end(),
                                  {channel(.77f), channel(.73f), channel(.64f), 255});
    }
    return pixels;
}
} // namespace ergo::studio::app
