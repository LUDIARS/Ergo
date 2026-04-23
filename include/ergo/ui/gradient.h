#pragma once

/// ergo::ui::gradient — small CPU gradient painter.
///
/// Produces a standalone `Bitmap` filled with a linear or radial
/// gradient, independently of any SVG. Useful when the frame SVG
/// supplies just the border and you want to lay a gradient *background*
/// behind it at the target size (the SVG's own gradient would stretch
/// with the center band of 9-slice, which is usually fine but sometimes
/// gives a visible seam where edges meet center — a separate backdrop
/// avoids that entirely).
///
/// Two blend modes match how UI toolkits normally paint gradients: 8-bit
/// per-channel RGBA, pre-multiplied alpha **not** assumed. Caller
/// composites the result as they prefer.

#include <cstdint>
#include <vector>

#include "ergo/ui/bitmap.h"

namespace ergo::ui {

/// RGBA8 gradient stop at normalised position (0 = start, 1 = end).
struct GradientStop {
    float   pos = 0.0f;   ///< clamped to [0, 1]
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

/// Gradient type — mirror the common CSS / SVG lexicon.
enum class GradientKind : uint8_t {
    Linear = 0,
    Radial = 1,
};

struct GradientSpec {
    GradientKind             kind = GradientKind::Linear;
    /// At least two stops required. Positions are sorted internally.
    std::vector<GradientStop> stops;

    // ── Linear parameters ──
    /// Angle in degrees. 0 = left→right, 90 = top→bottom,
    /// 180 = right→left, 270 = bottom→top. Matches CSS `linear-gradient`.
    float angle_deg = 90.0f;

    // ── Radial parameters ──
    /// Centre of the radial gradient in normalised coords (0..1).
    float cx = 0.5f;
    float cy = 0.5f;
    /// Radius at which `stops.back().pos==1` is hit, in normalised
    /// space (1.0 = half the longer axis). Must be > 0.
    float radius = 0.5f;
};

/// Render the gradient into a fresh `width × height` RGBA8 `Bitmap`.
/// Returns an invalid Bitmap when `stops.size() < 2` or dimensions
/// are zero.
Bitmap compose_gradient(const GradientSpec& spec,
                        uint32_t width,
                        uint32_t height);

} // namespace ergo::ui
