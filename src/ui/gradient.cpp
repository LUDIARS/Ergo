#include "ergo/ui/gradient.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ergo::ui {

namespace {

/// Lookup a gradient colour at normalised `t` (clamped to [0,1]).
/// `stops` must have ≥ 2 elements and be sorted by `pos` ascending.
void sample(const std::vector<GradientStop>& stops, float t,
            uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    if (t <= stops.front().pos) {
        const auto& s = stops.front();
        r = s.r; g = s.g; b = s.b; a = s.a; return;
    }
    if (t >= stops.back().pos) {
        const auto& s = stops.back();
        r = s.r; g = s.g; b = s.b; a = s.a; return;
    }
    for (std::size_t i = 1; i < stops.size(); ++i) {
        const auto& lo = stops[i-1];
        const auto& hi = stops[i];
        if (t <= hi.pos) {
            const float span = hi.pos - lo.pos;
            const float k    = span > 0.0f ? (t - lo.pos) / span : 0.0f;
            auto lerp = [&](uint8_t a0, uint8_t b0) {
                return static_cast<uint8_t>(std::lround(a0 + (b0 - a0) * k));
            };
            r = lerp(lo.r, hi.r);
            g = lerp(lo.g, hi.g);
            b = lerp(lo.b, hi.b);
            a = lerp(lo.a, hi.a);
            return;
        }
    }
}

} // namespace

Bitmap compose_gradient(const GradientSpec& spec, uint32_t w, uint32_t h) {
    if (spec.stops.size() < 2 || w == 0 || h == 0) return {};

    // Copy + sort stops so callers can pass them unsorted (CSS-style
    // authoring convenience).
    std::vector<GradientStop> stops = spec.stops;
    for (auto& s : stops) s.pos = std::clamp(s.pos, 0.0f, 1.0f);
    std::stable_sort(stops.begin(), stops.end(),
                     [](const GradientStop& a, const GradientStop& b) { return a.pos < b.pos; });

    Bitmap out = Bitmap::zeros(w, h);
    uint8_t* dp = out.rgba.data();

    if (spec.kind == GradientKind::Linear) {
        // Build a direction vector from the angle.
        const float rad = spec.angle_deg * 3.14159265358979f / 180.0f;
        const float dx  = std::cos(rad);
        const float dy  = std::sin(rad);

        // Compute the scalar along the axis for every pixel, normalise
        // to [0, 1] using the projection range defined by the 4 corners.
        // That way the gradient extends from the "earliest" corner to
        // the "latest" corner for any angle.
        auto proj = [&](float x, float y) { return x * dx + y * dy; };
        const float corners[4] = {
            proj(0.0f,       0.0f),
            proj(float(w-1), 0.0f),
            proj(0.0f,       float(h-1)),
            proj(float(w-1), float(h-1)),
        };
        float pmin = corners[0], pmax = corners[0];
        for (int i = 1; i < 4; ++i) {
            pmin = std::min(pmin, corners[i]);
            pmax = std::max(pmax, corners[i]);
        }
        const float pspan = pmax - pmin;
        const float inv_span = pspan > 0.0f ? 1.0f / pspan : 0.0f;

        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const float t = (proj(float(x), float(y)) - pmin) * inv_span;
                uint8_t r8, g8, b8, a8;
                sample(stops, t, r8, g8, b8, a8);
                uint8_t* p = dp + (static_cast<std::size_t>(y) * w + x) * 4u;
                p[0] = r8; p[1] = g8; p[2] = b8; p[3] = a8;
            }
        }
    } else {
        // Radial. Normalise pixel coords to [0, 1] and measure distance
        // from (cx, cy). `radius` is in the same normalised space,
        // clamped below to avoid divide-by-zero.
        const float inv_w = w > 1 ? 1.0f / float(w - 1) : 1.0f;
        const float inv_h = h > 1 ? 1.0f / float(h - 1) : 1.0f;
        const float radius = std::max(spec.radius, 1e-4f);
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const float nx = float(x) * inv_w;
                const float ny = float(y) * inv_h;
                const float dx = nx - spec.cx;
                const float dy = ny - spec.cy;
                const float d  = std::sqrt(dx * dx + dy * dy);
                const float t  = d / radius;
                uint8_t r8, g8, b8, a8;
                sample(stops, t, r8, g8, b8, a8);
                uint8_t* p = dp + (static_cast<std::size_t>(y) * w + x) * 4u;
                p[0] = r8; p[1] = g8; p[2] = b8; p[3] = a8;
            }
        }
    }
    return out;
}

} // namespace ergo::ui
