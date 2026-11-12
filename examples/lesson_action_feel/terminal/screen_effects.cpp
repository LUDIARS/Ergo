#include "screen_effects.h"

#include "screen.h"

#include <cmath>
#include <cstdlib>

namespace lesson_action_feel::terminal {

namespace {

constexpr float kPi = 3.14159265f;

char line_glyph(float dx, float dy) {   // screen y grows downwards
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0f) return 0;
    const float c = dx / len;
    const float s = dy / len;
    if (std::fabs(c) > 0.92f) return '-';
    if (std::fabs(s) > 0.92f) return '|';
    return c * s > 0.0f ? '\\' : '/';
}

/// Cells from the body along (sx, sy) columns / rows, without the body's own cell.
template <class Glyph>
std::vector<GlyphCell> along(float sx, float sy, Glyph glyph) {
    std::vector<GlyphCell> out;
    const int n = static_cast<int>(std::lround(std::fmax(std::fabs(sx), std::fabs(sy))));
    for (int k = 1; k <= n; ++k) {
        const float f = static_cast<float>(k) / static_cast<float>(n);
        GlyphCell   c{static_cast<int>(std::lround(sx * f)), static_cast<int>(std::lround(sy * f)), glyph(k, n)};
        if ((c.dc != 0 || c.dr != 0) && (out.empty() || out.back().dc != c.dc || out.back().dr != c.dr)) out.push_back(c);
    }
    return out;
}

} // namespace

char boss_blur_glyph(int dx_cols, int dy_rows, float strength, BlurKind kind) {
    if (kind == BlurKind::None || strength < 0.05f) return 0;
    const float dx = static_cast<float>(dx_cols) / static_cast<float>(kColsPerUnit);
    const float dy = static_cast<float>(dy_rows);
    const float d  = std::sqrt(dx * dx + dy * dy);
    if (d < 1.5f) return 0;   // the boss itself stays clear
    if (kind == BlurKind::Radial && d > 1.5f + strength * (kBlurRadius - 1.5f)) return 0;
    if (kind == BlurKind::Zoom && d < kBlurRadius * (1.0f - strength)) return 0;
    const int   lines = strength < 0.5f ? 8 : 16;
    const float step  = 2.0f * kPi / static_cast<float>(lines);
    const float a     = std::atan2(dy, dx);
    const float k     = std::round(a / step);
    const float line  = k * step;
    if (std::fabs(a - line) * d > 0.35f) return 0;   // off the line by more than about one cell
    return line_glyph(std::cos(line), std::sin(line));
}

std::vector<GlyphCell> trail_cells(Vec2 streak) {
    return along(streak.x * kColsPerUnit, streak.y, [](int k, int n) {
        return 3 * k <= n ? '%' : (3 * k <= 2 * n ? ';' : ',');
    });
}

std::vector<GlyphCell> vector_cells(Vec2 motion) {
    const float sx   = motion.x * kVectorScale * kColsPerUnit;
    const float sy   = motion.y * kVectorScale;
    const char  body = line_glyph(sx, sy);
    char        head = body;
    if (body == '-') head = sx > 0 ? '>' : '<';
    else if (body == '|') head = sy > 0 ? 'v' : '^';
    return along(sx, sy, [body, head](int k, int n) { return k == n ? head : body; });
}

} // namespace lesson_action_feel::terminal
