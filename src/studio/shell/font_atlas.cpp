#include "ergo/studio/shell/font_atlas.h"

#include "pictor/text/font_loader.h"
#include "pictor/text/text_rasterizer.h"

#include <algorithm>
#include <stdexcept>
#include <set>

namespace ergo::studio::shell {

FontAtlas::FontAtlas()
    : loader_(std::make_unique<pictor::FontLoader>()), raster_(std::make_unique<pictor::TextRasterizer>(*loader_)) {}

FontAtlas::~FontAtlas() = default;

bool FontAtlas::load(const std::string& path, std::string& err) {
    const pictor::FontHandle h = loader_->load_from_file(path);
    if (h == pictor::INVALID_FONT) {
        err = "font not readable: " + path;
        return false;
    }
    pictor::TextRasterizer::Config cfg;
    cfg.font_size    = raster_size_;
    cfg.atlas_width  = 2048;
    cfg.atlas_height = 2048;
    const auto set = static_cast<pictor::CharSet>(static_cast<uint32_t>(pictor::CharSet::ASCII) |
                                                  static_cast<uint32_t>(pictor::CharSet::LATIN_EXTENDED) |
                                                  static_cast<uint32_t>(pictor::CharSet::HIRAGANA) |
                                                  static_cast<uint32_t>(pictor::CharSet::KATAKANA) |
                                                  static_cast<uint32_t>(pictor::CharSet::SYMBOLS));
    // Pictor throws for outlines it cannot rasterise (e.g. CFF): report, do not crash.
    try {
        if (!raster_->build_atlas(h, set, cfg)) {
            err = "glyph atlas could not be built for " + path;
            return false;
        }
    } catch (const std::exception& e) {
        err = "font " + path + " cannot be rasterised: " + e.what();
        return false;
    }
    font_   = h;
    loaded_ = true;
    return true;
}

std::vector<uint32_t> FontAtlas::decode_utf8(const std::string& s) {
    std::vector<uint32_t> out;
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        uint32_t cp  = 0;
        int      n   = 0;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
        else if ((c >> 3) == 30) { cp = c & 0x07; n = 4; }
        else { ++i; continue; }   // stray continuation byte
        if (i + static_cast<std::size_t>(n) > s.size()) break;
        for (int k = 1; k < n; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        out.push_back(cp);
        i += static_cast<std::size_t>(n);
    }
    return out;
}

void FontAtlas::ensure(const std::string& utf8) const {
    if (!loaded()) return;
    std::set<uint32_t> missing;
    for (uint32_t cp : decode_utf8(utf8)) {
        if (cp >= 0x20 && !raster_->has_glyph(cp) && loader_->has_glyph(font_, cp)) missing.insert(cp);
    }
    if (missing.empty()) return;
    std::vector<pictor::CodepointRange> ranges;
    for (uint32_t cp : missing) ranges.push_back({cp, cp});
    try {
        raster_->add_codepoints(font_, ranges);
    } catch (const std::exception&) {
        // A glyph that cannot be rasterised is drawn as a gap (same advance as a missing glyph).
    }
}

const pictor::GlyphAtlasEntry* FontAtlas::glyph(uint32_t cp) const { return raster_->get_glyph(cp); }
const pictor::ImageBuffer*     FontAtlas::page(uint32_t index) const { return raster_->get_page(index); }

float FontAtlas::width(const std::string& utf8, float font_size) const {
    if (!loaded()) return 0;
    ensure(utf8);
    const float scale = font_size / raster_size_;
    float w = 0;
    for (uint32_t cp : decode_utf8(utf8)) {
        const auto* g = raster_->get_glyph(cp);
        w += g ? g->metrics.advance_x * scale : font_size * 0.5f;   // same advance as Pictor for a missing glyph
    }
    return w;
}

float FontAtlas::line_height(float font_size) const {
    const pictor::FontMetrics* fm = loaded() ? loader_->get_metrics(font_) : nullptr;
    if (!fm || fm->units_per_em == 0) return font_size * 1.3f;
    return fm->line_height * (font_size / static_cast<float>(fm->units_per_em));
}

float FontAtlas::ascender(float font_size) const {
    const pictor::FontMetrics* fm = loaded() ? loader_->get_metrics(font_) : nullptr;
    if (!fm || fm->units_per_em == 0) return font_size * 0.8f;
    return fm->ascender * (font_size / static_cast<float>(fm->units_per_em));
}

} // namespace ergo::studio::shell
