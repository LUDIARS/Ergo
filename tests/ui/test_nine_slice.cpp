#include "gtest/gtest.h"

#include <array>
#include <cstring>

#include "ergo/ui/nine_slice.h"

using namespace ergo::ui;

namespace {

/// 8x8 test bitmap with distinct colour per 9-slice region (insets 3/3/3/3).
/// Row-major, top-down, RGBA8. Corners get saturated primaries, edges get
/// mid tones, center gets grey — easy to verify pixel origin from bytes.
Bitmap make_labelled_8x8() {
    Bitmap b = Bitmap::zeros(8, 8);
    auto put = [&](uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t bl) {
        auto i = b.offset(x, y);
        b.rgba[i+0] = r; b.rgba[i+1] = g; b.rgba[i+2] = bl; b.rgba[i+3] = 255;
    };
    for (uint32_t y = 0; y < 8; ++y) {
        for (uint32_t x = 0; x < 8; ++x) {
            const bool left   = x < 3;
            const bool right  = x >= 5;
            const bool top    = y < 3;
            const bool bottom = y >= 5;
            if      (top && left)    put(x, y, 255, 0, 0);
            else if (top && right)   put(x, y, 0, 255, 0);
            else if (bottom && left) put(x, y, 0, 0, 255);
            else if (bottom && right)put(x, y, 255, 255, 0);
            else if (top)            put(x, y, 180, 40,  40);  // top edge
            else if (bottom)         put(x, y, 40,  40, 180);  // bottom edge
            else if (left)           put(x, y, 40, 180,  40);  // left edge
            else if (right)          put(x, y, 180, 180, 40);  // right edge
            else                     put(x, y, 128, 128, 128); // center
        }
    }
    return b;
}

} // namespace

TEST(NineSlice, CanFitCornersReportsCapacity) {
    SliceInsets ins{3, 3, 3, 3};
    EXPECT_TRUE (can_fit_corners(ins, 16, 16));
    EXPECT_TRUE (can_fit_corners(ins, 6, 6));   // exactly corners, empty centre
    EXPECT_FALSE(can_fit_corners(ins, 5, 6));   // 1 px too narrow
    EXPECT_FALSE(can_fit_corners(ins, 6, 5));   // 1 px too short
}

TEST(NineSlice, IdentityRoundTripByteForByte) {
    auto src = make_labelled_8x8();
    auto out = compose_nine_slice(src, {3, 3, 3, 3}, 8, 8);
    ASSERT_TRUE(out.valid());
    EXPECT_EQ(out.rgba, src.rgba);
}

// Inline byte comparison helper — avoids lambdas that confused MSVC's
// template deduction in the mini-gtest `auto&&` macro expansion.
static void expect_pixel_eq(const Bitmap& a, uint32_t ax, uint32_t ay,
                            const Bitmap& b, uint32_t bx, uint32_t by) {
    const auto i = a.offset(ax, ay);
    const auto j = b.offset(bx, by);
    EXPECT_EQ(a.rgba[i+0], b.rgba[j+0]);
    EXPECT_EQ(a.rgba[i+1], b.rgba[j+1]);
    EXPECT_EQ(a.rgba[i+2], b.rgba[j+2]);
    EXPECT_EQ(a.rgba[i+3], b.rgba[j+3]);
}

TEST(NineSlice, CornersPreservedAtAnyTargetSize) {
    auto src = make_labelled_8x8();
    auto out = compose_nine_slice(src, {3, 3, 3, 3}, 20, 14);
    ASSERT_TRUE(out.valid());

    // All 4 corner pixels should match src's corners byte-for-byte.
    expect_pixel_eq(out, 0, 0,                          src, 0, 0);
    expect_pixel_eq(out, out.width - 1, 0,              src, src.width - 1, 0);
    expect_pixel_eq(out, 0, out.height - 1,             src, 0, src.height - 1);
    expect_pixel_eq(out, out.width - 1, out.height - 1, src, src.width - 1, src.height - 1);

    // A sample 2px inside TL corner (still within inset.left=3, inset.top=3)
    // comes from the same src coords — no translation in the corner band.
    expect_pixel_eq(out, 2, 2, src, 2, 2);
}

TEST(NineSlice, TopEdgeStretchesOnlyHorizontally) {
    auto src = make_labelled_8x8();
    auto out = compose_nine_slice(src, {3, 3, 3, 3}, 20, 14);
    ASSERT_TRUE(out.valid());

    // Top edge band: y < inset.top, x in center zone (inset.left .. tw - right)
    // All pixels there originate from src's top-edge strip (180, 40, 40).
    for (uint32_t dy = 0; dy < 3; ++dy) {
        for (uint32_t dx = 3; dx < out.width - 3; ++dx) {
            const auto i = out.offset(dx, dy);
            EXPECT_EQ(out.rgba[i+0], 180u);
            EXPECT_EQ(out.rgba[i+1], 40u);
            EXPECT_EQ(out.rgba[i+2], 40u);
        }
    }
}

TEST(NineSlice, CenterBandIsStretchedMidGrey) {
    auto src = make_labelled_8x8();
    auto out = compose_nine_slice(src, {3, 3, 3, 3}, 20, 14);
    ASSERT_TRUE(out.valid());

    // Pick a representative center pixel and check its RGB (128, 128, 128).
    const uint32_t cx = out.width / 2;
    const uint32_t cy = out.height / 2;
    const auto i = out.offset(cx, cy);
    EXPECT_EQ(out.rgba[i+0], 128u);
    EXPECT_EQ(out.rgba[i+1], 128u);
    EXPECT_EQ(out.rgba[i+2], 128u);
}

TEST(NineSlice, ZeroInsetsDegenerateToPureStretch) {
    auto src = make_labelled_8x8();
    auto out = compose_nine_slice(src, {0, 0, 0, 0}, 4, 4);
    ASSERT_TRUE(out.valid());
    EXPECT_EQ(out.width, 4u);
    EXPECT_EQ(out.height, 4u);
    // With 0 insets the whole image is the center band — a nearest-neighbor
    // resize. Can't assert exact values without re-implementing the sampler
    // here, but we can assert we produced SOMETHING non-black.
    uint32_t nonzero = 0;
    for (uint8_t v : out.rgba) if (v) ++nonzero;
    EXPECT_GT(nonzero, 0u);
}

TEST(NineSlice, RejectsInsetsLargerThanSource) {
    auto src = make_labelled_8x8();
    std::string err;
    auto out = compose_nine_slice(src, {5, 5, 5, 5}, 20, 20, SliceFillMode::Stretch, &err);
    EXPECT_FALSE(out.valid());
    EXPECT_NE(err.find("src.width"), std::string::npos);
}

TEST(NineSlice, RejectsTargetSmallerThanCorners) {
    auto src = make_labelled_8x8();
    std::string err;
    auto out = compose_nine_slice(src, {3, 3, 3, 3}, 4, 4, SliceFillMode::Stretch, &err);
    EXPECT_FALSE(out.valid());
    EXPECT_NE(err.find("corner"), std::string::npos);
}

TEST(NineSlice, TileModeReturnsErrorForNow) {
    auto src = make_labelled_8x8();
    std::string err;
    auto out = compose_nine_slice(src, {3, 3, 3, 3}, 20, 14, SliceFillMode::Tile, &err);
    EXPECT_FALSE(out.valid());
    EXPECT_NE(err.find("tile"), std::string::npos);
}

TEST(NineSlice, ExactCornerFitProducesOnlyCorners) {
    // target = sum of corners on both axes — center band shrinks to zero.
    auto src = make_labelled_8x8();
    auto out = compose_nine_slice(src, {3, 3, 3, 3}, 6, 6);
    ASSERT_TRUE(out.valid());
    EXPECT_EQ(out.width,  6u);
    EXPECT_EQ(out.height, 6u);
}
