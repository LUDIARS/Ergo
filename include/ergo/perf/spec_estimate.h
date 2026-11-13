#pragma once

/// ergo_perf - the first quality step for this PC, before any frame is timed.
///
/// The caller describes the machine (the GPU's name as the graphics API gives
/// it, VRAM, discrete or integrated, the resolution) and, if it has one, a
/// short timing taken at start-up. The estimate predicts the full-quality
/// frame cost and picks the first ladder step expected to fit the budget; the
/// governor corrects it from real frames afterwards.
///
/// Full spec: spec/feature/module/perf.md

#include <cstdint>
#include <string>
#include <vector>

namespace ergo::perf {

/// A family of GPUs by a part of its name ("GTX 1070", "RTX 40").
struct GpuClass {
    std::string name_contains;  ///< matched case-insensitively; the first match wins
    double relative_power = 1.0;  ///< 1: the reference GPU; 2: twice as fast
};

struct HardwareProbe {
    std::string gpu_name;
    std::uint64_t vram_mb = 0;  ///< 0: unknown
    bool discrete = true;
    std::uint32_t width = 0;    ///< the render resolution (0: the reference)
    std::uint32_t height = 0;
    double measured_cost_ms = 0.0;  ///< a start-up timing (0: none taken)
    int measured_at_step = 0;       ///< the ladder step that timing was taken at
};

struct SpecEstimateConfig {
    double reference_cost_ms = 12.0;  ///< the full-quality cost on the reference GPU at the reference size
    std::uint32_t reference_width = 1920;
    std::uint32_t reference_height = 1080;
    double fixed_fraction = 0.3;      ///< the share of the cost that does not grow with pixels
    std::vector<GpuClass> classes;
    double unknown_discrete_power = 1.0;
    double unknown_integrated_power = 0.35;
    std::uint64_t low_vram_mb = 0;    ///< below this, the power is scaled by low_vram_scale (0: off)
    double low_vram_scale = 0.8;
    double step_saving = 0.08;        ///< the share of the cost one ladder step is expected to take off
};

struct SpecEstimate {
    double power = 1.0;              ///< relative to the reference GPU
    bool matched_class = false;      ///< a GpuClass named the GPU
    double predicted_cost_ms = 0.0;  ///< at full quality
    int step = 0;                    ///< the first step expected to fit `target_ms`
};

[[nodiscard]] SpecEstimate estimate_initial_step(const HardwareProbe& probe, const SpecEstimateConfig& config,
                                                 double target_ms, int step_count);

} // namespace ergo::perf
