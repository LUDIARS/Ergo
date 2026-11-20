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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ergo/ui/bitmap.h"

namespace ergo::ui {

struct SvgRasterOptions {
    /// DPI hint for unit-less SVG measurements. 96 matches the CSS default.
    float dpi = 96.0f;
    /// SVG source larger than this is refused before nanoSVG sees it (UI art
    /// is kilobytes; 16 MiB leaves room for embedded data).
    std::size_t max_source_bytes = 16u * 1024 * 1024;
    /// Each target side: 16384 is the common GPU texture limit (Vulkan
    /// maxImageDimension2D on desktop GPUs); nanoSVG takes int sizes.
    uint32_t max_side = 16384;
    /// width * height * 4, checked before the buffer is allocated
    /// (256 MiB = 8192 x 8192 RGBA8).
    std::size_t max_bitmap_bytes = 256u * 1024 * 1024;
};

/// Rasterise an SVG file. On failure (unreadable or over-limit source, a
/// target size beyond the options' limits, a parse error) returns an empty
/// Bitmap and (if non-null) writes the reason to `out_err`.
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
