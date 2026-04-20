#include "ergo/gpu_particle/curve.h"
#include "gtest/gtest.h"

#include <cmath>

using namespace ergo::gpu_particle;

TEST(Curve, ConstantEvaluatesEverywhere) {
    Curve c = Curve::constant(3.14f);
    EXPECT_NEAR(c.evaluate(0.0f), 3.14f, 1e-6f);
    EXPECT_NEAR(c.evaluate(0.5f), 3.14f, 1e-6f);
    EXPECT_NEAR(c.evaluate(1.0f), 3.14f, 1e-6f);
    EXPECT_NEAR(c.evaluate(-1.0f), 3.14f, 1e-6f);  // clamped
    EXPECT_NEAR(c.evaluate( 5.0f), 3.14f, 1e-6f);
}

TEST(Curve, LinearInterpolates) {
    Curve c = Curve::linear(0.0f, 10.0f);
    EXPECT_NEAR(c.evaluate(0.00f),  0.0f, 1e-5f);
    EXPECT_NEAR(c.evaluate(0.25f),  2.5f, 1e-5f);
    EXPECT_NEAR(c.evaluate(0.50f),  5.0f, 1e-5f);
    EXPECT_NEAR(c.evaluate(0.75f),  7.5f, 1e-5f);
    EXPECT_NEAR(c.evaluate(1.00f), 10.0f, 1e-5f);
}

TEST(Curve, BakedSamplesMatchEvaluate) {
    Curve c = Curve::linear(1.0f, 5.0f);
    Curve::BakedArray samples;
    c.bake(samples);
    for (uint32_t i = 0; i < Curve::kSampleCount; ++i) {
        const float t = float(i) / float(Curve::kSampleCount - 1);
        EXPECT_NEAR(samples[i], c.evaluate(t), 1e-5f);
    }
}

TEST(Curve, AddKeyStaysSorted) {
    Curve c;
    c.add_key(0.8f, 4.0f);
    c.add_key(0.2f, 1.0f);
    c.add_key(0.5f, 2.0f);
    const auto& k = c.keys();
    ASSERT_EQ(k.size(), 3u);
    EXPECT_LT(k[0].time, k[1].time);
    EXPECT_LT(k[1].time, k[2].time);
}

TEST(MinMaxCurve, TwoConstantsLerpsWithRand) {
    MinMaxCurve mm = MinMaxCurve::two_constants(2.0f, 10.0f);
    EXPECT_NEAR(mm.evaluate(0.0f, 0.0f),  2.0f, 1e-5f);
    EXPECT_NEAR(mm.evaluate(0.0f, 1.0f), 10.0f, 1e-5f);
    EXPECT_NEAR(mm.evaluate(0.0f, 0.5f),  6.0f, 1e-5f);
}

TEST(MinMaxCurve, ClampsRandTo01) {
    MinMaxCurve mm = MinMaxCurve::two_constants(0.0f, 1.0f);
    EXPECT_NEAR(mm.evaluate(0.0f, -5.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(mm.evaluate(0.0f,  5.0f), 1.0f, 1e-5f);
}
