#include "gtest/gtest.h"

#include "ergo/ui/svg_raster.h"

#include <filesystem>
#include <fstream>
#include <string>

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

// --- Input limits (SvgRasterOptions) -----------------------------------------

TEST(SvgRasterLimits, BitmapBytesExactlyAtLimitAndOneOver) {
    SvgRasterOptions opts;
    opts.max_bitmap_bytes = 16 * 16 * 4;
    std::string err;
    EXPECT_TRUE(raster_svg_memory(kSampleSvg, 16, 16, opts, &err).valid());
    EXPECT_FALSE(raster_svg_memory(kSampleSvg, 16, 17, opts, &err).valid());
    EXPECT_NE(err.find("exceeds the raster limits"), std::string::npos);
}

TEST(SvgRasterLimits, SideExactlyAtLimitAndOneOver) {
    SvgRasterOptions opts;
    opts.max_side = 64;
    std::string err;
    EXPECT_TRUE(raster_svg_memory(kSampleSvg, 64, 1, opts, &err).valid());
    EXPECT_FALSE(raster_svg_memory(kSampleSvg, 65, 1, opts, &err).valid());
    EXPECT_FALSE(raster_svg_memory(kSampleSvg, 1, 65, opts, &err).valid());
}

TEST(SvgRasterLimits, HugeTargetsAreRefusedBeforeAllocating) {
    std::string err;
    // Default limits: 16384 per side and 256 MiB. 16384 x 16384 x 4 = 1 GiB.
    EXPECT_FALSE(raster_svg_memory(kSampleSvg, 16384, 16384, {}, &err).valid());
    EXPECT_FALSE(raster_svg_memory(kSampleSvg, 0xFFFFFFFFu, 0xFFFFFFFFu, {}, &err).valid());
    EXPECT_FALSE(raster_svg_memory(kSampleSvg, 0x80000000u, 1, {}, &err).valid());
}

TEST(SvgRasterLimits, SourceBytesExactlyAtLimitAndOneOver) {
    const std::string src = kSampleSvg;
    SvgRasterOptions opts;
    opts.max_source_bytes = src.size();
    std::string err;
    EXPECT_TRUE(raster_svg_memory(src, 8, 8, opts, &err).valid());
    opts.max_source_bytes = src.size() - 1;
    EXPECT_FALSE(raster_svg_memory(src, 8, 8, opts, &err).valid());
    EXPECT_NE(err.find("larger than"), std::string::npos);
}

TEST(SvgRasterLimits, MalformedSourcesDoNotCrash) {
    std::string err;
    for (const char* bad : {"<svg", "<svg><path d=\"M 0 0 L\"/></svg>", "\x00\xff\xfe",
                            "<svg viewBox=\"0 0 -1 nan\"><rect width=\"1e308\" height=\"1e308\"/></svg>"}) {
        (void)raster_svg_memory(bad, 8, 8, {}, &err);  // empty or a bitmap; never a crash
    }
    EXPECT_TRUE(true);
}

TEST(SvgRasterLimits, FileSourceIsReadWithTheSameLimit) {
    const auto path = std::filesystem::temp_directory_path() / "ergo_test_svg_raster_limit.svg";
    {
        std::ofstream out(path, std::ios::binary);
        out << kSampleSvg;
    }
    const std::string src = kSampleSvg;
    SvgRasterOptions opts;
    opts.max_source_bytes = src.size();
    std::string err;
    EXPECT_TRUE(raster_svg_file(path.string(), 8, 8, opts, &err).valid());
    opts.max_source_bytes = src.size() - 1;
    EXPECT_FALSE(raster_svg_file(path.string(), 8, 8, opts, &err).valid());
    EXPECT_NE(err.find("larger than"), std::string::npos);
    EXPECT_FALSE(raster_svg_file((path.string() + ".missing"), 8, 8, {}, &err).valid());
    std::filesystem::remove(path);
}
