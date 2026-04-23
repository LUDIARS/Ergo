#include "ergo/ui/nine_slice.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace ergo::ui {

bool can_fit_corners(SliceInsets i, uint32_t tw, uint32_t th) noexcept {
    return tw >= i.left + i.right
        && th >= i.top  + i.bottom;
}

namespace {

// Map a destination x to a source x via 9-slice zoning.
//   dst_x < left                       → sx = dst_x                   (TL/BL/Left corner/edge)
//   dst_x in [target-right, target)    → sx = src_w - (target - dst_x)
//   else (center band)                 → linear stretch in [left, src_w - right)
inline uint32_t map_axis(uint32_t dst, uint32_t target, uint32_t src_dim,
                         uint32_t near_inset, uint32_t far_inset) {
    if (dst < near_inset) {
        return dst;
    }
    if (dst >= target - far_inset) {
        return src_dim - (target - dst);
    }
    // Center band. Use integer math where possible — avoid fp for determinism.
    const uint32_t dst_span = target - near_inset - far_inset;
    const uint32_t src_span = src_dim - near_inset - far_inset;
    if (dst_span == 0 || src_span == 0) {
        return near_inset; // degenerate — map to inset boundary
    }
    // `(dst - near) * src_span / dst_span` with a tiny bias to avoid
    // sampling the far edge when dst_span < src_span.
    const uint64_t num = static_cast<uint64_t>(dst - near_inset) * src_span;
    const uint32_t rel = static_cast<uint32_t>(num / dst_span);
    uint32_t sx = near_inset + rel;
    if (sx >= src_dim - far_inset) sx = src_dim - far_inset - 1;
    return sx;
}

bool validate(const Bitmap& src, SliceInsets i, uint32_t tw, uint32_t th, std::string* err) {
    if (!src.valid()) {
        if (err) *err = "nine_slice: source bitmap invalid";
        return false;
    }
    if (i.left + i.right >= src.width) {
        if (err) *err = "nine_slice: insets left+right must be < src.width";
        return false;
    }
    if (i.top + i.bottom >= src.height) {
        if (err) *err = "nine_slice: insets top+bottom must be < src.height";
        return false;
    }
    if (!can_fit_corners(i, tw, th)) {
        if (err) *err = "nine_slice: target smaller than corner footprint";
        return false;
    }
    return true;
}

} // namespace

Bitmap compose_nine_slice(const Bitmap& src,
                          SliceInsets insets,
                          uint32_t tw, uint32_t th,
                          SliceFillMode mode,
                          std::string* err) {
    if (mode != SliceFillMode::Stretch) {
        // Tile mode deliberately unimplemented — document the upgrade path.
        if (err) *err = "nine_slice: tile mode not yet supported";
        return {};
    }
    if (!validate(src, insets, tw, th, err)) return {};

    // Identity fast path: the output is a byte-for-byte copy when the
    // target equals the source size. Exercised by the test suite as the
    // canonical round-trip invariant.
    if (tw == src.width && th == src.height) {
        return src;
    }

    Bitmap out = Bitmap::zeros(tw, th);

    const uint32_t sw = src.width;
    const uint32_t sh = src.height;
    const uint8_t* sp = src.rgba.data();
    uint8_t*       dp = out.rgba.data();

    for (uint32_t dy = 0; dy < th; ++dy) {
        const uint32_t sy = map_axis(dy, th, sh, insets.top, insets.bottom);
        const uint8_t* srow = sp + static_cast<std::size_t>(sy) * sw * 4u;
        uint8_t*       drow = dp + static_cast<std::size_t>(dy) * tw * 4u;
        for (uint32_t dx = 0; dx < tw; ++dx) {
            const uint32_t sx = map_axis(dx, tw, sw, insets.left, insets.right);
            // nearest-neighbor sample; 4 bytes per pixel.
            std::memcpy(drow + dx * 4, srow + sx * 4, 4);
        }
    }
    return out;
}

} // namespace ergo::ui
