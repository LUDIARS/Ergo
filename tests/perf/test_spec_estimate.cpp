#include "gtest/gtest.h"

#include "ergo/perf/spec_estimate.h"

using namespace ergo::perf;

namespace {

/// The reference: a GTX 1070 that spends 12 ms at 1280x720 at full quality.
SpecEstimateConfig config() {
    SpecEstimateConfig c;
    c.reference_cost_ms = 12.0;
    c.reference_width = 1280;
    c.reference_height = 720;
    c.fixed_fraction = 0.3;
    c.classes = {{"GTX 1070", 1.0}, {"RTX 4090", 4.0}, {"GTX 1050", 0.4}};
    c.step_saving = 0.1;
    return c;
}

HardwareProbe gpu(const char* name, std::uint32_t w, std::uint32_t h) {
    HardwareProbe p;
    p.gpu_name = name;
    p.width = w;
    p.height = h;
    return p;
}

} // namespace

TEST(SpecEstimate, ReferenceGpuAtReferenceSizeStartsAtFullQuality) {
    const SpecEstimate e = estimate_initial_step(gpu("NVIDIA GeForce GTX 1070", 1280, 720), config(), 16.6, 10);
    EXPECT_TRUE(e.matched_class);
    EXPECT_NEAR(e.predicted_cost_ms, 12.0, 1e-9);
    EXPECT_EQ(e.step, 0);
}

TEST(SpecEstimate, MorePixelsStartLighter) {
    const SpecEstimate hd = estimate_initial_step(gpu("nvidia geforce gtx 1070", 1920, 1080), config(), 16.6, 10);
    // 12 x (0.3 + 0.7 x 2.25) = 22.5 ms; 0.9^3 x 22.5 = 16.4 <= 16.6.
    EXPECT_NEAR(hd.predicted_cost_ms, 22.5, 1e-9);
    EXPECT_EQ(hd.step, 3);
}

TEST(SpecEstimate, FasterGpuStaysFullAndSlowerCapsAtTheLadderEnd) {
    // 1440p on a GPU four times the reference: 12 x (0.3 + 0.7 x 4) / 4 = 9.3 ms.
    EXPECT_EQ(estimate_initial_step(gpu("NVIDIA GeForce RTX 4090", 2560, 1440), config(), 16.6, 10).step, 0);
    // 4K is 6.6x the cost: even that GPU starts two steps down (19.8 ms x 0.81 = 16.0).
    EXPECT_EQ(estimate_initial_step(gpu("NVIDIA GeForce RTX 4090", 3840, 2160), config(), 16.6, 10).step, 2);
    const SpecEstimate slow = estimate_initial_step(gpu("GeForce GTX 1050", 3840, 2160), config(), 16.6, 4);
    EXPECT_EQ(slow.step, 4);
}

TEST(SpecEstimate, UnknownIntegratedGpuIsAssumedWeaker) {
    HardwareProbe p = gpu("Some iGPU", 1280, 720);
    p.discrete = false;
    const SpecEstimate e = estimate_initial_step(p, config(), 16.6, 10);
    EXPECT_FALSE(e.matched_class);
    EXPECT_NEAR(e.power, 0.35, 1e-9);
    EXPECT_GT(e.step, 0);
}

TEST(SpecEstimate, LowVramScalesThePower) {
    SpecEstimateConfig c = config();
    c.low_vram_mb = 4096;
    c.low_vram_scale = 0.5;
    HardwareProbe p = gpu("GTX 1070", 1280, 720);
    p.vram_mb = 2048;
    EXPECT_NEAR(estimate_initial_step(p, c, 16.6, 10).power, 0.5, 1e-9);
}

TEST(SpecEstimate, AMeasurementBeatsTheName) {
    HardwareProbe p = gpu("NVIDIA GeForce RTX 4090", 1280, 720);
    p.measured_cost_ms = 20.0;  // measured at full quality: the name's guess is overruled
    const SpecEstimate e = estimate_initial_step(p, config(), 16.6, 10);
    EXPECT_NEAR(e.predicted_cost_ms, 20.0, 1e-9);
    EXPECT_EQ(e.step, 2);  // 20 x 0.81 = 16.2
}
