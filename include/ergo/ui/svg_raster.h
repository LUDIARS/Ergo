#pragma once

/// ergo::ui::svg_raster — parse + rasterise SVG into a `Bitmap`.
///
/// Thin wrapper around nanoSVG. The source is either a file path or an
/// in-memory buffer; the output is always RGBA8. Aspect-ratio of the
/// SVG's own `viewBox` is preserved: if the target dims differ from the
/// SVG's native size, `scale = min(target.w / src.w, target.h / src.h)`
/// is applied and the result is drawn top-left without padding.
///
/// No Vulkan or Pictor dependency — the result lives in a plain
/// `std::vector<uint8_t>` inside `Bitmap`.

#include <cstdint>
#include <string>
#include <string_view>

#include "ergo/ui/bitmap.h"

namespace ergo::ui {

struct SvgRasterOptions {
    /// DPI hint for unit-less SVG measurements. 96 matches the CSS default.
    float dpi = 96.0f;
};

/// Rasterise an SVG file. On failure returns an empty Bitmap and (if
/// non-null) writes the nanoSVG error message to `out_err`.
Bitmap raster_svg_file(const std::string& svg_path,
                       uint32_t target_w,
                       uint32_t target_h,
                       const SvgRasterOptions& opts = {},
                       std::string* out_err = nullptr);

/// Rasterise an in-memory SVG document. `src` may be any
/// null-terminated-or-not byte span — the parser makes its own writable
/// copy so `src` need not outlive the call.
Bitmap raster_svg_memory(std::string_view svg_src,
                         uint32_t target_w,
                         uint32_t target_h,
                         const SvgRasterOptions& opts = {},
                         std::string* out_err = nullptr);

} // namespace ergo::ui
