#pragma once

/// ergo_ui — headless 2D image / 9-slice utility.
///
/// `Bitmap` is the one 2D buffer type the whole module hands around.
/// Row-major, top-down, 4 bytes per pixel (R, G, B, A). No premultiplied
/// alpha — callers mix that in themselves if they need it. Purely CPU;
/// Vulkan / Pictor integration lives elsewhere.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ergo::ui {

struct Bitmap {
    uint32_t             width  = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> rgba;   ///< size == width * height * 4

    bool valid() const noexcept {
        return width > 0 && height > 0 &&
               rgba.size() == static_cast<std::size_t>(width) * height * 4u;
    }

    /// Zero-initialized RGBA8 buffer of the given dimensions.
    static Bitmap zeros(uint32_t w, uint32_t h) {
        Bitmap b;
        b.width  = w;
        b.height = h;
        b.rgba.assign(static_cast<std::size_t>(w) * h * 4u, 0);
        return b;
    }

    /// Byte offset of pixel (x, y) in `rgba`. No bounds check.
    std::size_t offset(uint32_t x, uint32_t y) const noexcept {
        return (static_cast<std::size_t>(y) * width + x) * 4u;
    }
};

} // namespace ergo::ui
