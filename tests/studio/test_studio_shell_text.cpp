// TextCanvas / FontAtlas with a real font (CPU only, no GPU). The font comes
// from ERGO_STUDIO_TEST_FONT; without it the test reports and skips.
#include "ergo/studio/shell/font_atlas.h"
#include "ergo/studio/shell/text_canvas.h"

#include <gtest/gtest.h>
#include "pictor/text/text_types.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace ergo::studio::shell;

namespace {
std::string test_font() {
    const char* f = std::getenv("ERGO_STUDIO_TEST_FONT");
    return f ? f : "";
}
} // namespace

TEST(StudioShellText, DrawsVisibleGlyphs) {
    const std::string path = test_font();
    if (path.empty()) {
        std::printf("  ERGO_STUDIO_TEST_FONT not set: skipped\n");
        return;
    }
    FontAtlas font;
    std::string err;
    ASSERT_TRUE(font.load(path, err));
    EXPECT_GT(font.width("Ab", 16), 10.0f);
    EXPECT_GT(font.width("\xE6\xA6\x82\xE8\xA6\x81", 16), 20.0f);   // 概要 (kanji added on demand)

    {
        const auto* g = font.glyph('A');
        ASSERT_NE(g, nullptr);
        const auto* pg = font.page(g->atlas_page);
        ASSERT_NE(pg, nullptr);
        std::size_t nz = 0;
        for (auto v : pg->pixels) nz += v != 0;
        std::size_t in_box = 0;
        const auto x0 = static_cast<uint32_t>(g->uv_x0 * pg->width), x1 = static_cast<uint32_t>(g->uv_x1 * pg->width);
        const auto y0 = static_cast<uint32_t>(g->uv_y0 * pg->height), y1 = static_cast<uint32_t>(g->uv_y1 * pg->height);
        for (uint32_t y = y0; y < y1; ++y) {
            for (uint32_t x = x0; x < x1; ++x) in_box += pg->pixels[(static_cast<std::size_t>(y) * pg->width + x) * pg->channels] != 0;
        }
        std::printf("  A box %u..%u x %u..%u: nonzero %zu\n", x0, x1, y0, y1, in_box);
        std::printf("  A: w=%g h=%g bx=%g by=%g adv=%g uv=(%g,%g)-(%g,%g) page=%u %ux%u ch=%u nonzero=%zu\n",
                    g->metrics.width, g->metrics.height, g->metrics.bearing_x, g->metrics.bearing_y, g->metrics.advance_x,
                    g->uv_x0, g->uv_y0, g->uv_x1, g->uv_y1, g->atlas_page, pg->width, pg->height, pg->channels, nz);
    }
    for (uint32_t cp : {static_cast<uint32_t>('E'), static_cast<uint32_t>('L'), static_cast<uint32_t>('-')}) {
        const auto* g  = font.glyph(cp);
        const auto* pg = g ? font.page(g->atlas_page) : nullptr;
        if (!pg) continue;
        const auto x0 = static_cast<uint32_t>(g->uv_x0 * pg->width), x1 = static_cast<uint32_t>(g->uv_x1 * pg->width);
        const auto y0 = static_cast<uint32_t>(g->uv_y0 * pg->height), y1 = static_cast<uint32_t>(g->uv_y1 * pg->height);
        std::printf("  '%c' %ux%u (metrics %gx%g):\n", static_cast<char>(cp), x1 - x0, y1 - y0, g->metrics.width, g->metrics.height);
        for (uint32_t y = y0; y < y1; ++y) {
            std::string row;
            for (uint32_t x = x0; x < x1; ++x) {
                const auto v = pg->pixels[(static_cast<std::size_t>(y) * pg->width + x) * pg->channels];
                row.push_back(v > 160 ? '#' : v > 40 ? '+' : '.');
            }
            std::printf("    %s\n", row.c_str());
        }
    }
    TextCanvas canvas;
    canvas.resize(200, 60);
    ergo::ui_kit::DrawCmd c;
    c.kind      = ergo::ui_kit::DrawKind::Text;
    c.rect      = {10, 10, 180, 24};
    c.text      = "Ab \xE6\xA6\x82\xE8\xA6\x81";
    c.font_size = 18;
    ASSERT_TRUE(canvas.render({c}, font));
    std::size_t lit = 0;
    for (std::size_t i = 3; i < canvas.pixels().size(); i += 4) {
        if (canvas.pixels()[i] > 128) ++lit;
    }
    std::size_t any = 0;
    int         mx  = 0;
    for (std::size_t i = 3; i < canvas.pixels().size(); i += 4) {
        any += canvas.pixels()[i] != 0;
        mx = std::max<int>(mx, canvas.pixels()[i]);
    }
    std::printf("  lit pixels: %zu (nonzero %zu, max alpha %d) line=%g asc=%g\n", lit, any, mx, font.line_height(18),
                font.ascender(18));
    // Anti-aliased strokes are ~1.5 px wide, so many pixels are partly covered.
    EXPECT_GT(any, 200u);
    EXPECT_GT(mx, 150);
    EXPECT_FALSE(canvas.render({c}, font));   // unchanged list: no redraw
}
