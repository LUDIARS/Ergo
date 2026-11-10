#pragma once
/// The Studio's font: Pictor FontLoader + TextRasterizer, measuring text for
/// layout (ui::ITextMeasure) and supplying glyph bitmaps to TextCanvas.
/// ASCII, Latin, kana and symbols are rasterised up front; other code points
/// (kanji) are added the first time they are measured or drawn.
#include "ergo/studio/ui/view_builder.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pictor {
class FontLoader;
class TextRasterizer;
struct GlyphAtlasEntry;
struct ImageBuffer;
} // namespace pictor

namespace ergo::studio::shell {

class FontAtlas final : public ui::ITextMeasure {
public:
    FontAtlas();
    ~FontAtlas() override;
    FontAtlas(const FontAtlas&)            = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    /// TTF / OTF / TTC (first face). False with `err` when it cannot be read.
    bool load(const std::string& path, std::string& err);
    bool loaded() const { return loaded_; }

    float width(const std::string& utf8, float font_size) const override;
    float line_height(float font_size) const override;
    float ascender(float font_size) const;

    /// Rasterise the code points of `utf8` missing from the atlas.
    void ensure(const std::string& utf8) const;
    const pictor::GlyphAtlasEntry* glyph(uint32_t cp) const;
    const pictor::ImageBuffer*     page(uint32_t index) const;
    /// Size the atlas was rasterised at (glyph metrics are in these pixels).
    float raster_size() const { return raster_size_; }

    static std::vector<uint32_t> decode_utf8(const std::string& s);

private:
    std::unique_ptr<pictor::FontLoader>     loader_;
    std::unique_ptr<pictor::TextRasterizer> raster_;
    uint32_t                                font_        = 0;      ///< valid only when loaded_ (handle 0 is a real font)
    bool                                    loaded_      = false;
    float                                   raster_size_ = 20.0f;   ///< close to UI sizes (13-20 px): less blur when shrinking
};

} // namespace ergo::studio::shell
