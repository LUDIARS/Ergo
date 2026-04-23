#pragma once

/// ergo::ui::line_glow — travelling highlight along a frame's border.
///
/// Takes an already-composed frame `Bitmap` at the target size and
/// overlays a "chasing" highlight along its perimeter or top/bottom
/// edges. Each call produces a **single frame** — the caller drives
/// the animation by passing increasing `time_sec` values and
/// re-uploading the returned bitmap to its texture every frame.
///
/// The glow is an additive or normal-alpha strip of colour drawn on
/// top of the existing pixel data. The source is left untouched unless
/// the caller opts into in-place composition.

#include <cstdint>

#include "ergo/ui/bitmap.h"

namespace ergo::ui {

/// Which path the glow traces.
enum class GlowPath : uint8_t {
    /// Clockwise around the outer rectangle: left-edge top→bottom is
    /// the travel direction on the left side, so the "head" visibly
    /// flows left→right across the top. Natural fit for a UI panel
    /// highlight that continuously circulates.
    Perimeter = 0,
    /// Two parallel horizontal sweeps on the top and bottom edges,
    /// both moving left→right. The vertical edges are untouched.
    Horizontal = 1,
};

/// Blend mode for the overlay.
enum class GlowBlend : uint8_t {
    /// out = min(src + glow * alpha, 255). Intense neon look.
    Additive = 0,
    /// out = glow * a + src * (1 - a). Softer, matches CSS paint.
    Normal   = 1,
};

struct LineGlowSpec {
    GlowPath  path        = GlowPath::Perimeter;
    GlowBlend blend       = GlowBlend::Additive;

    /// Glow colour (pre-alpha). The alpha channel is driven by the
    /// falloff curve — this is the *peak* colour at the head.
    uint8_t r = 200, g = 220, b = 255;

    /// Maximum alpha at the glow head (0..255). Decays linearly along
    /// `head_length_px` pixels.
    uint8_t peak_alpha = 200;

    /// Thickness of the glow band normal to the travel direction, in
    /// target pixels. 1 = exact border line, larger values glow
    /// outward and inward symmetrically.
    uint32_t thickness_px = 2;

    /// Length of the comet tail in pixels along the travel axis.
    uint32_t head_length_px = 48;

    /// Seconds for one complete loop. Negative values reverse direction.
    float period_seconds = 2.0f;

    /// Phase offset in the [0, 1) loop fraction. Lets multiple frames
    /// have glows out of sync (e.g. title bar glow vs body glow).
    float phase = 0.0f;
};

/// Composite a glow animation frame onto `src` and return a new
/// Bitmap. `src` is not modified. Returns invalid Bitmap when `src`
/// is invalid.
Bitmap overlay_line_glow(const Bitmap& src,
                         const LineGlowSpec& spec,
                         float time_sec);

/// In-place variant for callers that own `dst` and want to avoid an
/// extra allocation each frame. Returns false + leaves `dst` unchanged
/// on invalid input.
bool overlay_line_glow_into(Bitmap& dst,
                            const LineGlowSpec& spec,
                            float time_sec);

} // namespace ergo::ui
