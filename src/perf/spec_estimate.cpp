#include "ergo/perf/spec_estimate.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ergo::perf {
namespace {

std::string lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// The GPU's power relative to the reference, and whether a class named it.
double gpu_power(const HardwareProbe& probe, const SpecEstimateConfig& config, bool& matched) {
    const std::string name = lower(probe.gpu_name);
    double power = probe.discrete ? config.unknown_discrete_power : config.unknown_integrated_power;
    matched = false;
    for (const GpuClass& c : config.classes) {
        if (c.name_contains.empty()) continue;
        if (name.find(lower(c.name_contains)) != std::string::npos) {
            power = c.relative_power;
            matched = true;
            break;
        }
    }
    if (config.low_vram_mb > 0 && probe.vram_mb > 0 && probe.vram_mb < config.low_vram_mb) {
        power *= config.low_vram_scale;
    }
    return std::max(power, 1e-3);
}

/// How the cost grows with the resolution against the reference size.
double pixel_scale(const HardwareProbe& probe, const SpecEstimateConfig& config) {
    const double ref = static_cast<double>(config.reference_width) * static_cast<double>(config.reference_height);
    if (probe.width == 0 || probe.height == 0 || ref <= 0.0) return 1.0;
    const double ratio = static_cast<double>(probe.width) * static_cast<double>(probe.height) / ref;
    const double fixed = std::clamp(config.fixed_fraction, 0.0, 1.0);
    return fixed + (1.0 - fixed) * ratio;
}

} // namespace

SpecEstimate estimate_initial_step(const HardwareProbe& probe, const SpecEstimateConfig& config, double target_ms,
                                   int step_count) {
    SpecEstimate out;
    out.power = gpu_power(probe, config, out.matched_class);
    const double keep = std::clamp(1.0 - config.step_saving, 0.05, 1.0);
    if (probe.measured_cost_ms > 0.0) {
        // A timing beats a name: carry it back to full quality.
        out.predicted_cost_ms =
            probe.measured_cost_ms / std::pow(keep, static_cast<double>(std::max(probe.measured_at_step, 0)));
    } else {
        out.predicted_cost_ms = config.reference_cost_ms * pixel_scale(probe, config) / out.power;
    }
    const int last = std::max(step_count, 0);
    out.step = last;
    double cost = out.predicted_cost_ms;
    for (int step = 0; step <= last; ++step) {
        if (cost <= target_ms) {
            out.step = step;
            break;
        }
        cost *= keep;
    }
    return out;
}

} // namespace ergo::perf
