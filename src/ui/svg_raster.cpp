#include "ergo/ui/svg_raster.h"

#include <algorithm>
#include <string>

// nanoSVG is header-only. Its `_IMPLEMENTATION` macros must be defined
// in exactly one TU — this file owns them.
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "nanosvg/nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg/nanosvgrast.h"

namespace ergo::ui {

namespace {

struct RasterGuard {
    NSVGrasterizer* r = nullptr;
    RasterGuard() : r(nsvgCreateRasterizer()) {}
    ~RasterGuard() { if (r) nsvgDeleteRasterizer(r); }
};
struct ImageGuard {
    NSVGimage* img = nullptr;
    ~ImageGuard() { if (img) nsvgDelete(img); }
};

Bitmap raster_impl(NSVGimage* img, uint32_t w, uint32_t h, std::string* err) {
    Bitmap out;
    if (!img) { if (err) *err = "ergo::ui::svg_raster: null image"; return out; }

    RasterGuard rast;
    if (!rast.r) { if (err) *err = "ergo::ui::svg_raster: rasterizer alloc failed"; return out; }

    out.width  = w;
    out.height = h;
    out.rgba.assign(static_cast<std::size_t>(w) * h * 4u, 0);

    // Preserve aspect ratio — pick the smaller axis scale so the full
    // SVG fits in the target box without clipping.
    const float sx = static_cast<float>(w) / std::max(1.0f, img->width);
    const float sy = static_cast<float>(h) / std::max(1.0f, img->height);
    const float scale = std::min(sx, sy);

    nsvgRasterize(rast.r, img, 0.0f, 0.0f, scale,
                  out.rgba.data(),
                  static_cast<int>(w),
                  static_cast<int>(h),
                  static_cast<int>(w) * 4);
    return out;
}

} // namespace

Bitmap raster_svg_file(const std::string& path,
                       uint32_t w, uint32_t h,
                       const SvgRasterOptions& opts,
                       std::string* err) {
    ImageGuard g;
    g.img = nsvgParseFromFile(path.c_str(), "px", opts.dpi);
    if (!g.img) { if (err) *err = "ergo::ui::svg_raster: failed to parse " + path; return {}; }
    return raster_impl(g.img, w, h, err);
}

Bitmap raster_svg_memory(std::string_view src,
                         uint32_t w, uint32_t h,
                         const SvgRasterOptions& opts,
                         std::string* err) {
    // nsvgParse is destructive — make a writable copy.
    std::string buf(src);
    ImageGuard g;
    g.img = nsvgParse(buf.data(), "px", opts.dpi);
    if (!g.img) { if (err) *err = "ergo::ui::svg_raster: failed to parse in-memory SVG"; return {}; }
    return raster_impl(g.img, w, h, err);
}

} // namespace ergo::ui
