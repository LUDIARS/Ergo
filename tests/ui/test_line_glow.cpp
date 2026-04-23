#include "gtest/gtest.h"

#include "ergo/ui/line_glow.h"

using namespace ergo::ui;

namespace {

Bitmap make_black(uint32_t w, uint32_t h) {
    // Alpha = 255 so we can see additive brightening clearly.
    auto b = Bitmap::zeros(w, h);
    for (std::size_t i = 3; i < b.rgba.size(); i += 4) b.rgba[i] = 255;
    return b;
}

bool any_nonzero_rgb(const Bitmap& b, uint32_t x, uint32_t y) {
    const auto i = b.offset(x, y);
    return b.rgba[i+0] || b.rgba[i+1] || b.rgba[i+2];
}

} // namespace

TEST(LineGlow, PerimeterAtTimeZeroLightsTopLeft) {
    auto src = make_black(32, 16);
    LineGlowSpec g;
    g.path           = GlowPath::Perimeter;
    g.head_length_px = 8;
    g.thickness_px   = 1;
    g.peak_alpha     = 255;
    g.period_seconds = 4.0f;
    auto out = overlay_line_glow(src, g, 0.0f);
    ASSERT_TRUE(out.valid());
    // At t=0, head_f==0 → head is at index 0 == top-left corner.
    // Pixel (0,0) should be non-black; a pixel far away (bottom-right
    // center) should still be untouched.
    EXPECT_TRUE (any_nonzero_rgb(out, 0, 0));
    EXPECT_FALSE(any_nonzero_rgb(out, 16, 8));
}

TEST(LineGlow, PerimeterHeadMovesOverTime) {
    auto src = make_black(32, 16);
    LineGlowSpec g;
    g.path           = GlowPath::Perimeter;
    g.head_length_px = 2;
    g.thickness_px   = 1;
    g.peak_alpha     = 255;
    g.period_seconds = 2.0f;

    auto a = overlay_line_glow(src, g, 0.0f);
    auto b = overlay_line_glow(src, g, 0.5f);
    ASSERT_TRUE(a.valid());
    ASSERT_TRUE(b.valid());
    // At t=0 the head is at index 0 (top-left). At t=0.5s (1/4 of
    // period) it's a quarter around the perimeter — no longer at (0,0),
    // so a → b should differ somewhere.
    EXPECT_NE(a.rgba, b.rgba);
}

TEST(LineGlow, PeriodLoopsOver) {
    auto src = make_black(40, 40);
    LineGlowSpec g;
    g.path           = GlowPath::Perimeter;
    g.head_length_px = 4;
    g.thickness_px   = 1;
    g.peak_alpha     = 255;
    g.period_seconds = 1.0f;

    auto a = overlay_line_glow(src, g, 0.0f);
    auto b = overlay_line_glow(src, g, 1.0f);   // exactly one loop later
    auto c = overlay_line_glow(src, g, 2.0f);
    // All three should be identical — the head position wraps to the
    // same fractional phase.
    EXPECT_EQ(a.rgba, b.rgba);
    EXPECT_EQ(a.rgba, c.rgba);
}

TEST(LineGlow, HorizontalOnlyTouchesTopAndBottomRows) {
    auto src = make_black(64, 16);
    LineGlowSpec g;
    g.path           = GlowPath::Horizontal;
    g.head_length_px = 32;
    g.thickness_px   = 1;
    g.peak_alpha     = 255;
    g.period_seconds = 2.0f;
    auto out = overlay_line_glow(src, g, 0.25f);
    ASSERT_TRUE(out.valid());

    // Middle rows should be completely untouched.
    for (uint32_t y = 4; y < 12; ++y) {
        for (uint32_t x = 0; x < 64; ++x) {
            EXPECT_FALSE(any_nonzero_rgb(out, x, y));
        }
    }
    // But somewhere on the top and bottom rows, some pixel should glow.
    bool lit_top = false, lit_bot = false;
    for (uint32_t x = 0; x < 64; ++x) {
        if (any_nonzero_rgb(out, x, 0))  lit_top = true;
        if (any_nonzero_rgb(out, x, 15)) lit_bot = true;
    }
    EXPECT_TRUE(lit_top);
    EXPECT_TRUE(lit_bot);
}

TEST(LineGlow, ThicknessExpandsBeamInward) {
    // Use a wide head (covers the full top edge) and pick a sample
    // pixel directly below the top edge but away from the corners.
    // With thickness=1, that inner pixel stays dark; with thickness=5,
    // the beam reaches two rows down.
    auto src = make_black(40, 8);
    LineGlowSpec thin = {}, thick = {};
    thin.path = thick.path = GlowPath::Perimeter;
    thin.head_length_px = thick.head_length_px = 256; // covers the whole perimeter
    thin.peak_alpha = thick.peak_alpha = 255;
    thin.period_seconds = thick.period_seconds = 2.0f;
    thin.thickness_px  = 1;
    thick.thickness_px = 5;

    auto a = overlay_line_glow(src, thin,  0.0f);
    auto b = overlay_line_glow(src, thick, 0.0f);
    ASSERT_TRUE(a.valid());
    ASSERT_TRUE(b.valid());

    // (20, 2) is 2 rows inside the top edge at x=20 — never on the
    // path itself. Thin (1 px) can't reach it; thick (5 px) does.
    EXPECT_FALSE(any_nonzero_rgb(a, 20, 2));
    EXPECT_TRUE (any_nonzero_rgb(b, 20, 2));
}

TEST(LineGlow, InvalidBitmapReturnsInvalid) {
    Bitmap empty;
    auto out = overlay_line_glow(empty, {}, 0.0f);
    EXPECT_FALSE(out.valid());
}

TEST(LineGlow, InPlaceVariantMutatesTarget) {
    auto src = make_black(16, 16);
    auto original = src.rgba;
    LineGlowSpec g;
    g.path = GlowPath::Perimeter;
    g.head_length_px = 4;
    g.thickness_px = 1;
    g.peak_alpha = 200;
    g.period_seconds = 1.0f;
    EXPECT_TRUE(overlay_line_glow_into(src, g, 0.0f));
    EXPECT_NE(src.rgba, original);
}
