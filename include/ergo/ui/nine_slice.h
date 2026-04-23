#pragma once

/// ergo::ui::nine_slice — classic "9-slice" / "9-patch" frame composer.
///
/// A source bitmap is logically cut into 9 regions by two vertical lines
/// (at `left` / `width-right`) and two horizontal lines (at `top` /
/// `height-bottom`). The 4 corners render **unscaled** (pixel-for-pixel)
/// at the target's matching corners, the 4 edges stretch along their
/// free axis, and the single center region stretches on both axes.
/// The result is a UI frame that can expand to any size without blurring
/// the decorative corners.
///
/// This is the same algorithm every UI toolkit (Flutter, Unity UGUI,
/// Godot StyleBoxTexture, ...) ships as built-in. We do it in RGBA8 with
/// nearest-neighbour sampling — no alpha-blending compositor, the caller
/// is expected to upload the output as a texture and render with its own
/// sampler. For pixel-art frames nearest is what you want; for smoother
/// hand-drawn frames, raster the SVG at a higher resolution before
/// slicing so the corners are already anti-aliased.

#include <cstdint>
#include <string>

#include "ergo/ui/bitmap.h"

namespace ergo::ui {

/// Corner sizes, in source-bitmap pixels. `left + right` must be
/// ≤ `src.width`, `top + bottom` ≤ `src.height`. Equal values imply
/// a symmetric frame; all zeros degenerates to a pure stretch.
struct SliceInsets {
    uint32_t left   = 0;
    uint32_t top    = 0;
    uint32_t right  = 0;
    uint32_t bottom = 0;
};

enum class SliceFillMode : uint8_t {
    /// Linear stretch along the free axis / axes. The default, and the
    /// only mode supported today.
    Stretch = 0,
    /// Repeat the edge/center region. Reserved for a future phase.
    Tile    = 1,
};

/// Compose a `target_w × target_h` bitmap from `src` using 9-slice
/// expansion defined by `insets`. Returns an empty Bitmap on invalid
/// insets (e.g. insets larger than source, target smaller than the
/// corners' combined footprint) and populates `out_err` when provided.
///
/// Valid inputs where `target_w == src.width && target_h == src.height`
/// yield a byte-for-byte copy of the source. This identity is a useful
/// round-trip invariant that the test suite exercises.
Bitmap compose_nine_slice(const Bitmap& src,
                          SliceInsets insets,
                          uint32_t target_w,
                          uint32_t target_h,
                          SliceFillMode mode = SliceFillMode::Stretch,
                          std::string* out_err = nullptr);

/// True iff the target dimensions can fit the fixed corners. Callers
/// that need to gracefully down-scale the frame should check this before
/// composing (e.g. fall back to a plain stretch).
bool can_fit_corners(SliceInsets insets,
                     uint32_t target_w,
                     uint32_t target_h) noexcept;

} // namespace ergo::ui
