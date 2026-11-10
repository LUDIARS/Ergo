#pragma once
/// CPU canvas holding every text run of a frame (straight-alpha RGBA). The
/// UI layer uploads it as one texture and draws it over the rectangles:
/// Pictor's UIRenderer has no UV per command, so glyphs cannot be drawn from
/// a shared atlas texture, and its 64-texture limit rules out one texture per
/// glyph or per string. Rebuilt only when the frame's text changes.
#include "ergo/studio/shell/font_atlas.h"
#include "ergo/ui_kit/ui_kit.h"

#include <cstdint>
#include <vector>

namespace ergo::studio::shell {

class TextCanvas {
public:
    void resize(uint32_t w, uint32_t h);
    uint32_t width() const { return w_; }
    uint32_t height() const { return h_; }
    const std::vector<uint8_t>& pixels() const { return px_; }

    /// Clear and draw every Text command of `list` (honouring PushClip /
    /// PopClip). Returns true when the result differs from the last call.
    bool render(const std::vector<ui_kit::DrawCmd>& list, const FontAtlas& font);

private:
    struct Clip {
        float x0, y0, x1, y1;
    };
    void draw_text(const ui_kit::DrawCmd& c, const FontAtlas& font, const Clip& clip);

    uint32_t             w_ = 0, h_ = 0;
    std::vector<uint8_t> px_;
    uint64_t             last_hash_ = 0;
};

/// Hash of the text-relevant part of a draw list (text, rect, colour, size, clips).
uint64_t text_signature(const std::vector<ui_kit::DrawCmd>& list);

} // namespace ergo::studio::shell
