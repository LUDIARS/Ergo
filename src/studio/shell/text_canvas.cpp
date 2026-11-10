#include "ergo/studio/shell/text_canvas.h"

#include "pictor/text/text_types.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ergo::studio::shell {
namespace {

uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

uint64_t fbits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

/// Mean alpha of the atlas area [sx0, sx1) x [sy0, sy1) (pixel units), each
/// source pixel weighted by its overlap. A box filter: the atlas is rasterised
/// larger than UI text, and point / bilinear sampling while shrinking skips
/// whole rows, dropping 1-px strokes (the bar of "E", "L", "-").
float area_alpha(const pictor::ImageBuffer& p, float sx0, float sy0, float sx1, float sy1) {
    const float w = sx1 - sx0, h = sy1 - sy0;
    if (w <= 0 || h <= 0) return 0;
    const int ix0 = std::max(0, static_cast<int>(std::floor(sx0)));
    const int iy0 = std::max(0, static_cast<int>(std::floor(sy0)));
    const int ix1 = std::min(static_cast<int>(p.width), static_cast<int>(std::ceil(sx1)));
    const int iy1 = std::min(static_cast<int>(p.height), static_cast<int>(std::ceil(sy1)));
    float sum = 0;
    for (int y = iy0; y < iy1; ++y) {
        const float oy = std::min(sy1, y + 1.0f) - std::max(sy0, static_cast<float>(y));
        if (oy <= 0) continue;
        for (int x = ix0; x < ix1; ++x) {
            const float ox = std::min(sx1, x + 1.0f) - std::max(sx0, static_cast<float>(x));
            if (ox <= 0) continue;
            const std::size_t i = (static_cast<std::size_t>(y) * p.width + static_cast<std::size_t>(x)) * p.channels + (p.channels - 1);
            sum += p.pixels[i] / 255.0f * ox * oy;
        }
    }
    return sum / (w * h);
}

} // namespace

uint64_t text_signature(const std::vector<ui_kit::DrawCmd>& list) {
    uint64_t h = 1469598103934665603ull;
    for (const auto& c : list) {
        if (c.kind != ui_kit::DrawKind::Text && c.kind != ui_kit::DrawKind::PushClip && c.kind != ui_kit::DrawKind::PopClip) {
            continue;
        }
        h = mix(h, static_cast<uint64_t>(c.kind));
        for (float f : {c.rect.x, c.rect.y, c.rect.w, c.rect.h, c.color.r, c.color.g, c.color.b, c.color.a, c.font_size, c.opacity}) {
            h = mix(h, fbits(f));
        }
        h = mix(h, static_cast<uint64_t>(c.text_align));
        for (unsigned char ch : c.text) h = mix(h, ch);
    }
    return h;
}

void TextCanvas::resize(uint32_t w, uint32_t h) {
    if (w == w_ && h == h_) return;
    w_ = w;
    h_ = h;
    px_.assign(static_cast<std::size_t>(w) * h * 4, 0);
    last_hash_ = 0;
}

bool TextCanvas::render(const std::vector<ui_kit::DrawCmd>& list, const FontAtlas& font) {
    const uint64_t sig = mix(text_signature(list), (static_cast<uint64_t>(w_) << 32) | h_);
    if (sig == last_hash_) return false;
    last_hash_ = sig;
    std::fill(px_.begin(), px_.end(), static_cast<uint8_t>(0));
    std::vector<Clip> clips{{0, 0, static_cast<float>(w_), static_cast<float>(h_)}};
    for (const auto& c : list) {
        if (c.kind == ui_kit::DrawKind::PushClip) {
            const Clip& top = clips.back();
            clips.push_back({std::max(top.x0, c.rect.x), std::max(top.y0, c.rect.y),
                             std::min(top.x1, c.rect.x + c.rect.w), std::min(top.y1, c.rect.y + c.rect.h)});
        } else if (c.kind == ui_kit::DrawKind::PopClip) {
            if (clips.size() > 1) clips.pop_back();
        } else if (c.kind == ui_kit::DrawKind::Text && !c.text.empty()) {
            draw_text(c, font, clips.back());
        }
    }
    return true;
}

void TextCanvas::draw_text(const ui_kit::DrawCmd& c, const FontAtlas& font, const Clip& clip) {
    if (!font.loaded()) return;
    font.ensure(c.text);
    const float scale = c.font_size / font.raster_size();
    const float text_w = font.width(c.text, c.font_size);
    float x = c.rect.x;
    if (c.text_align == ui_kit::TextAlign::Center) x += (c.rect.w - text_w) / 2;
    else if (c.text_align == ui_kit::TextAlign::Right) x += c.rect.w - text_w;
    // Centre the line box vertically in the node rect.
    const float line = font.line_height(c.font_size);
    const float base = c.rect.y + (c.rect.h - line) / 2 + font.ascender(c.font_size);
    const float alpha = c.color.a * c.opacity;
    const uint8_t cr = static_cast<uint8_t>(std::lround(std::min(1.0f, c.color.r) * 255));
    const uint8_t cg = static_cast<uint8_t>(std::lround(std::min(1.0f, c.color.g) * 255));
    const uint8_t cb = static_cast<uint8_t>(std::lround(std::min(1.0f, c.color.b) * 255));

    for (uint32_t cp : FontAtlas::decode_utf8(c.text)) {
        const auto* g = font.glyph(cp);
        if (!g) {
            x += c.font_size * 0.5f;
            continue;
        }
        const float gw = g->metrics.width * scale, gh = g->metrics.height * scale;
        const auto* page = font.page(g->atlas_page);
        if (gw > 0 && gh > 0 && page) {
            const float gx0 = x + g->metrics.bearing_x * scale;
            const float gy0 = base - g->metrics.bearing_y * scale;
            const float u0 = g->uv_x0 * page->width, v0 = g->uv_y0 * page->height;
            const float u1 = g->uv_x1 * page->width, v1 = g->uv_y1 * page->height;
            const int px0 = static_cast<int>(std::floor(std::max(gx0, clip.x0)));
            const int py0 = static_cast<int>(std::floor(std::max(gy0, clip.y0)));
            const int px1 = static_cast<int>(std::ceil(std::min(gx0 + gw, clip.x1)));
            const int py1 = static_cast<int>(std::ceil(std::min(gy0 + gh, clip.y1)));
            for (int py = std::max(py0, 0); py < std::min(py1, static_cast<int>(h_)); ++py) {
                for (int px = std::max(px0, 0); px < std::min(px1, static_cast<int>(w_)); ++px) {
                    // The destination pixel's footprint in the glyph (0..1), clamped to the glyph box.
                    const float tx0 = std::max(0.0f, (px - gx0) / gw), tx1 = std::min(1.0f, (px + 1 - gx0) / gw);
                    const float ty0 = std::max(0.0f, (py - gy0) / gh), ty1 = std::min(1.0f, (py + 1 - gy0) / gh);
                    if (tx1 <= tx0 || ty1 <= ty0) continue;
                    // Coverage of the whole pixel: the part outside the glyph box counts as empty.
                    const float cover = (tx1 - tx0) * gw * (ty1 - ty0) * gh;
                    const float a = area_alpha(*page, u0 + (u1 - u0) * tx0, v0 + (v1 - v0) * ty0, u0 + (u1 - u0) * tx1,
                                               v0 + (v1 - v0) * ty1) * cover * alpha;
                    if (a <= 0.002f) continue;
                    uint8_t* d = &px_[(static_cast<std::size_t>(py) * w_ + static_cast<std::size_t>(px)) * 4];
                    // "over" in straight alpha
                    const float da = d[3] / 255.0f;
                    const float oa = a + da * (1 - a);
                    auto blend = [&](uint8_t dst, uint8_t src) {
                        return static_cast<uint8_t>(std::lround((src * a + dst * da * (1 - a)) / std::max(oa, 1e-6f)));
                    };
                    d[0] = blend(d[0], cr);
                    d[1] = blend(d[1], cg);
                    d[2] = blend(d[2], cb);
                    d[3] = static_cast<uint8_t>(std::lround(oa * 255));
                }
            }
        }
        x += g->metrics.advance_x * scale;
    }
}

} // namespace ergo::studio::shell
