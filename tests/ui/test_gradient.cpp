#include "gtest/gtest.h"

#include "ergo/ui/gradient.h"

using namespace ergo::ui;

TEST(Gradient, LinearLeftToRightEndsMatchStops) {
    GradientSpec g;
    g.kind = GradientKind::Linear;
    g.angle_deg = 0.0f;          // x axis — left→right
    g.stops = {
        { 0.0f, 255, 0, 0, 255 },
        { 1.0f, 0, 0, 255, 255 },
    };
    auto bm = compose_gradient(g, 16, 4);
    ASSERT_TRUE(bm.valid());

    // leftmost pixel column ≈ stop 0 (pure red)
    const auto l = bm.offset(0, 2);
    EXPECT_EQ(bm.rgba[l+0], 255u);
    EXPECT_EQ(bm.rgba[l+2], 0u);
    // rightmost pixel column ≈ stop 1 (pure blue)
    const auto r = bm.offset(15, 2);
    EXPECT_EQ(bm.rgba[r+0], 0u);
    EXPECT_EQ(bm.rgba[r+2], 255u);
}

TEST(Gradient, LinearTopToBottomSweeps) {
    GradientSpec g;
    g.kind = GradientKind::Linear;
    g.angle_deg = 90.0f;         // y axis — top→bottom
    g.stops = {
        { 0.0f, 10, 10, 10, 255 },
        { 1.0f, 250, 250, 250, 255 },
    };
    auto bm = compose_gradient(g, 4, 16);
    ASSERT_TRUE(bm.valid());

    const auto top = bm.offset(2, 0);
    const auto bot = bm.offset(2, 15);
    EXPECT_LT(bm.rgba[top+0], bm.rgba[bot+0]);
    EXPECT_LT(bm.rgba[top+1], bm.rgba[bot+1]);
}

TEST(Gradient, RadialCentreHotterThanCorners) {
    GradientSpec g;
    g.kind   = GradientKind::Radial;
    g.cx     = 0.5f;
    g.cy     = 0.5f;
    g.radius = 0.5f;
    g.stops = {
        { 0.0f, 255, 255, 255, 255 },
        { 1.0f, 0,   0,   0,   255 },
    };
    auto bm = compose_gradient(g, 16, 16);
    ASSERT_TRUE(bm.valid());

    const auto centre = bm.offset(8, 8);
    const auto corner = bm.offset(0, 0);
    EXPECT_GT(bm.rgba[centre+0], bm.rgba[corner+0]);
    EXPECT_GT(bm.rgba[centre+1], bm.rgba[corner+1]);
    EXPECT_GT(bm.rgba[centre+2], bm.rgba[corner+2]);
}

TEST(Gradient, RejectsSingleStop) {
    GradientSpec g;
    g.stops = { { 0.5f, 127, 127, 127, 255 } };
    auto bm = compose_gradient(g, 8, 8);
    EXPECT_FALSE(bm.valid());
}

TEST(Gradient, RejectsZeroDimensions) {
    GradientSpec g;
    g.stops = {
        { 0.0f, 0, 0, 0, 255 },
        { 1.0f, 255, 255, 255, 255 },
    };
    auto bm = compose_gradient(g, 0, 8);
    EXPECT_FALSE(bm.valid());
    auto bm2 = compose_gradient(g, 8, 0);
    EXPECT_FALSE(bm2.valid());
}

TEST(Gradient, StopsSortedAutomatically) {
    // Caller passes stops in the wrong order — the impl should sort
    // them before sampling rather than producing garbage.
    GradientSpec g;
    g.kind = GradientKind::Linear;
    g.angle_deg = 0.0f;
    g.stops = {
        { 1.0f, 0,   0,   255, 255 },
        { 0.0f, 255, 0,   0,   255 },
    };
    auto bm = compose_gradient(g, 8, 2);
    ASSERT_TRUE(bm.valid());
    const auto l = bm.offset(0, 0);
    EXPECT_EQ(bm.rgba[l+0], 255u);  // left is still red despite reversed input
}
