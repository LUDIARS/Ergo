#include "ergo/ui/svg_raster.h"

#include <lorica/checked_size.hpp>

#include <algorithm>
#include <fstream>
#include <optional>
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

/// Target size check before anything is allocated: each side within
/// max_side (nanoSVG takes int sizes and an int stride) and w * h * 4 within
/// max_bitmap_bytes.
bool target_size_ok(uint32_t w, uint32_t h, const SvgRasterOptions& opts, std::string* err) {
    const auto pixels = lorica::checked_multiply(w, h);
    const auto bytes = pixels ? lorica::checked_multiply(*pixels, 4) : std::nullopt;
    if (w > opts.max_side || h > opts.max_side || !bytes || *bytes > opts.max_bitmap_bytes) {
        if (err) {
            *err = "ergo::ui::svg_raster: target " + std::to_string(w) + "x" + std::to_string(h)
                 + " exceeds the raster limits";
        }
        return false;
    }
    return true;
}

/// The whole file, or false when it cannot be read or is over the limit.
bool read_source(const std::string& path, std::size_t limit, std::string& out, std::string* err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { if (err) *err = "ergo::ui::svg_raster: failed to open " + path; return false; }
    out.clear();
    char chunk[64 * 1024];
    while (in) {
        in.read(chunk, sizeof(chunk));
        const auto got = static_cast<std::size_t>(in.gcount());
        if (got > limit - out.size()) {
            if (err) *err = "ergo::ui::svg_raster: " + path + " is larger than " + std::to_string(limit) + " bytes";
            return false;
        }
        out.append(chunk, got);
    }
    return true;
}

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
    if (!target_size_ok(w, h, opts, err)) return {};
    // Read with the size limit here instead of nsvgParseFromFile (which reads
    // any size); nsvgParse needs the terminating NUL std::string provides.
    std::string buf;
    if (!read_source(path, opts.max_source_bytes, buf, err)) return {};
    ImageGuard g;
    g.img = nsvgParse(buf.data(), "px", opts.dpi);
    if (!g.img) { if (err) *err = "ergo::ui::svg_raster: failed to parse " + path; return {}; }
    return raster_impl(g.img, w, h, err);
}

Bitmap raster_svg_memory(std::string_view src,
                         uint32_t w, uint32_t h,
                         const SvgRasterOptions& opts,
                         std::string* err) {
    if (!target_size_ok(w, h, opts, err)) return {};
    if (src.size() > opts.max_source_bytes) {
        if (err) *err = "ergo::ui::svg_raster: in-memory SVG is larger than " + std::to_string(opts.max_source_bytes) + " bytes";
        return {};
    }
    // nsvgParse is destructive — make a writable copy.
    std::string buf(src);
    ImageGuard g;
    g.img = nsvgParse(buf.data(), "px", opts.dpi);
    if (!g.img) { if (err) *err = "ergo::ui::svg_raster: failed to parse in-memory SVG"; return {}; }
    return raster_impl(g.img, w, h, err);
}

} // namespace ergo::ui
