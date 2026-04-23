#include "ergo/ui/line_glow.h"

#include <algorithm>
#include <cmath>

namespace ergo::ui {

namespace {

/// One glow-affected pixel: its target position in the bitmap plus the
/// 0..1 head-proximity factor (1.0 = at the head, fading toward 0).
struct GlowPixel {
    int32_t x;
    int32_t y;
    float   intensity;
};

inline void blend_pixel(uint8_t* p, uint8_t gr, uint8_t gg, uint8_t gb,
                        float alpha, GlowBlend mode) {
    const int32_t ai = static_cast<int32_t>(std::lround(alpha * 255.0f));
    if (ai <= 0) return;
    if (mode == GlowBlend::Additive) {
        const int32_t mul = ai;
        p[0] = static_cast<uint8_t>(std::min(255, p[0] + (gr * mul) / 255));
        p[1] = static_cast<uint8_t>(std::min(255, p[1] + (gg * mul) / 255));
        p[2] = static_cast<uint8_t>(std::min(255, p[2] + (gb * mul) / 255));
        p[3] = static_cast<uint8_t>(std::min(255, p[3] + ai));
    } else {
        // Normal alpha blend. `ai` is the glow alpha in 0..255.
        const int32_t ia = 255 - ai;
        p[0] = static_cast<uint8_t>((gr * ai + p[0] * ia) / 255);
        p[1] = static_cast<uint8_t>((gg * ai + p[1] * ia) / 255);
        p[2] = static_cast<uint8_t>((gb * ai + p[2] * ia) / 255);
        p[3] = static_cast<uint8_t>(std::min(255, p[3] + ai));
    }
}

/// Draw a thick strip around an ideal 1-pixel path point, normal to a
/// direction given by `(nx, ny)` (length 1).
inline void draw_thick_point(Bitmap& dst, int32_t x, int32_t y,
                             int32_t thickness, float nx, float ny,
                             uint8_t gr, uint8_t gg, uint8_t gb,
                             uint8_t peak_a, float intensity,
                             GlowBlend mode) {
    const int32_t W = static_cast<int32_t>(dst.width);
    const int32_t H = static_cast<int32_t>(dst.height);
    const int32_t span = std::max(1, thickness);
    // Symmetric expansion around the centre. Odd thicknesses centre
    // exactly on the border line; even thicknesses bias inward by half
    // a pixel (close enough for UI).
    const int32_t half = span / 2;
    for (int32_t k = -half; k <= span - 1 - half; ++k) {
        const int32_t px = x + static_cast<int32_t>(std::lround(nx * k));
        const int32_t py = y + static_cast<int32_t>(std::lround(ny * k));
        if (px < 0 || py < 0 || px >= W || py >= H) continue;
        uint8_t* p = dst.rgba.data() + (static_cast<std::size_t>(py) * W + px) * 4u;
        const float alpha = (peak_a / 255.0f) * intensity;
        blend_pixel(p, gr, gg, gb, alpha, mode);
    }
}

/// Apply the glow along a polyline path. `points` is the centre path
/// in clockwise order; `normals` gives the outward normal per point.
/// The head moves along the path; pixels within `head_len` behind it
/// receive a linearly-falling intensity.
void stroke_animated_path(Bitmap& dst,
                          const std::vector<std::pair<int32_t, int32_t>>& points,
                          const std::vector<std::pair<float, float>>& normals,
                          uint32_t head_len,
                          uint32_t thickness,
                          uint8_t gr, uint8_t gg, uint8_t gb, uint8_t peak_a,
                          GlowBlend blend,
                          float head_index_f) {
    const int32_t N = static_cast<int32_t>(points.size());
    if (N == 0) return;
    const int32_t hl = std::max(1, static_cast<int32_t>(head_len));

    // For each pixel on the path, figure out its distance *behind* the
    // head (positive) and intensity = 1 - d/head_len clamped to 0.
    const float head_f = head_index_f - std::floor(head_index_f); // 0..1
    const float head_abs = head_f * float(N);

    for (int32_t i = 0; i < N; ++i) {
        float d = head_abs - float(i);
        if (d < 0.0f) d += float(N);   // wrap — tail can wrap around
        if (d > float(hl)) continue;   // too far behind the head
        const float intensity = 1.0f - d / float(hl);
        draw_thick_point(dst, points[i].first, points[i].second,
                         static_cast<int32_t>(thickness),
                         normals[i].first, normals[i].second,
                         gr, gg, gb, peak_a, intensity, blend);
    }
}

/// Build the centre path + normals for the clockwise perimeter of an
/// axis-aligned rectangle (0,0)-(W-1,H-1). Perimeter walks:
///   top:    (0,0) → (W-1, 0)
///   right:  (W-1, 0) → (W-1, H-1)
///   bottom: (W-1, H-1) → (0, H-1)
///   left:   (0, H-1) → (0, 0)
/// So "head" starting at index 0 and increasing naturally moves
/// rightwards along the top — matching the user's "left to right" feel.
void build_perimeter(uint32_t w, uint32_t h,
                     std::vector<std::pair<int32_t, int32_t>>& pts,
                     std::vector<std::pair<float, float>>& nrm) {
    if (w < 2 || h < 2) return;
    const int32_t W = static_cast<int32_t>(w);
    const int32_t H = static_cast<int32_t>(h);
    // Normals point inward so thickness expands *into* the frame body.
    // (Outside is out-of-bounds for the overlay bitmap.)
    auto add = [&](int32_t x, int32_t y, float nx, float ny) {
        pts.emplace_back(x, y);
        nrm.emplace_back(nx, ny);
    };
    // top edge — normal pointing down
    for (int32_t x = 0; x < W - 1; ++x) add(x, 0, 0.0f, 1.0f);
    // right edge — normal pointing left
    for (int32_t y = 0; y < H - 1; ++y) add(W - 1, y, -1.0f, 0.0f);
    // bottom edge — normal pointing up
    for (int32_t x = W - 1; x > 0; --x) add(x, H - 1, 0.0f, -1.0f);
    // left edge — normal pointing right
    for (int32_t y = H - 1; y > 0; --y) add(0, y, 1.0f, 0.0f);
}

void build_horizontal(uint32_t w, uint32_t h,
                      std::vector<std::pair<int32_t, int32_t>>& pts,
                      std::vector<std::pair<float, float>>& nrm) {
    if (w < 2 || h == 0) return;
    const int32_t W = static_cast<int32_t>(w);
    // top edge L→R
    for (int32_t x = 0; x < W; ++x) {
        pts.emplace_back(x, 0);
        nrm.emplace_back(0.0f, 1.0f);
    }
    // bottom edge L→R (both sweep in the same direction)
    const int32_t yb = static_cast<int32_t>(h) - 1;
    for (int32_t x = 0; x < W; ++x) {
        pts.emplace_back(x, yb);
        nrm.emplace_back(0.0f, -1.0f);
    }
}

} // namespace

bool overlay_line_glow_into(Bitmap& dst, const LineGlowSpec& spec, float time_sec) {
    if (!dst.valid()) return false;

    std::vector<std::pair<int32_t, int32_t>> pts;
    std::vector<std::pair<float, float>>     nrm;
    if (spec.path == GlowPath::Horizontal) {
        build_horizontal(dst.width, dst.height, pts, nrm);
    } else {
        build_perimeter(dst.width, dst.height, pts, nrm);
    }
    if (pts.empty()) return false;

    // Convert time to a [0, 1) head position. `period_seconds == 0` or
    // negative values are tolerated: zero freezes the glow at `phase`,
    // negative reverses.
    float head_f;
    if (std::fabs(spec.period_seconds) < 1e-6f) {
        head_f = spec.phase;
    } else {
        head_f = spec.phase + time_sec / spec.period_seconds;
    }

    stroke_animated_path(dst, pts, nrm,
                        std::max(1u, spec.head_length_px),
                        std::max(1u, spec.thickness_px),
                        spec.r, spec.g, spec.b, spec.peak_alpha,
                        spec.blend,
                        head_f);
    return true;
}

Bitmap overlay_line_glow(const Bitmap& src, const LineGlowSpec& spec, float time_sec) {
    if (!src.valid()) return {};
    Bitmap out = src;
    (void)overlay_line_glow_into(out, spec, time_sec);
    return out;
}

} // namespace ergo::ui
