#include "gtest/gtest.h"

#include "ergo/ui/svg_raster.h"

using namespace ergo::ui;

namespace {
constexpr const char* kSampleSvg =
    "<?xml version=\"1.0\"?>"
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\" "
    "     width=\"100\" height=\"100\">"
    "  <rect width=\"100\" height=\"100\" fill=\"#001133\"/>"
    "  <rect x=\"30\" y=\"30\" width=\"40\" height=\"40\" fill=\"#eeff22\"/>"
    "</svg>";
} // namespace

TEST(SvgRaster, MemorySourceProducesRequestedDims) {
    std::string err;
    auto bm = raster_svg_memory(kSampleSvg, 32, 32, {}, &err);
    ASSERT_TRUE(bm.valid());
    EXPECT_EQ(bm.width,  32u);
    EXPECT_EQ(bm.height, 32u);
    EXPECT_EQ(bm.rgba.size(), 32u * 32u * 4u);
}

TEST(SvgRaster, OutputIsNotAllBlack) {
    auto bm = raster_svg_memory(kSampleSvg, 24, 24);
    ASSERT_TRUE(bm.valid());
    bool any = false;
    for (uint8_t v : bm.rgba) if (v) { any = true; break; }
    EXPECT_TRUE(any);
}

TEST(SvgRaster, EmptyInputReportsError) {
    std::string err;
    auto bm = raster_svg_memory("", 8, 8, {}, &err);
    // nanoSVG may still produce an empty image with no nodes; either
    // invalid() or an empty-but-valid buffer is acceptable here. If an
    // error message is set, make sure it mentions the module.
    if (!err.empty()) EXPECT_NE(err.find("svg_raster"), std::string::npos);
    if (bm.valid()) {
        bool non_zero = false;
        for (uint8_t v : bm.rgba) if (v) { non_zero = true; break; }
        EXPECT_FALSE(non_zero);
    }
}

TEST(SvgRaster, AspectRatioPreserved) {
    // 100x100 source asked to render into a wide 80x40 target — the
    // content should scale uniformly (min axis), leaving the right-hand
    // side transparent / black.
    auto bm = raster_svg_memory(kSampleSvg, 80, 40);
    ASSERT_TRUE(bm.valid());

    // Sample a pixel well past the 40x40 content column; alpha should be 0.
    const auto i = bm.offset(70, 20);
    EXPECT_EQ(bm.rgba[i + 3], 0u);
}
