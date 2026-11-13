#pragma once

/// ergo_perf - the frame's statistics: the CPU frame time and named stages
/// (GPU passes, CPU sections). Graphics-API agnostic: the caller measures
/// (a Vulkan timestamp query, a steady_clock delta) and hands in the stage's
/// name and its milliseconds.
///
/// Full spec: spec/feature/module/perf.md

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ergo/perf/rolling_series.h"

namespace ergo::perf {

class FrameStats {
public:
    /// `window`: frames each series keeps (240: four seconds at 60 fps).
    explicit FrameStats(std::size_t window = 240);

    /// One frame's time as the player sees it (between frames on the CPU).
    void add_frame(double cpu_ms);

    /// The id of stage `name`, made on first use (ids count up from 0 in that order).
    std::size_t stage(std::string_view name);
    /// The stage's time this frame.
    void add_stage(std::size_t id, double ms);
    void add_stage(std::string_view name, double ms) { add_stage(stage(name), ms); }

    [[nodiscard]] const RollingSeries& frame() const { return frame_; }
    [[nodiscard]] std::size_t stage_count() const { return stages_.size(); }
    [[nodiscard]] const std::string& stage_name(std::size_t id) const { return names_[id]; }
    [[nodiscard]] const RollingSeries& stage_series(std::size_t id) const { return stages_[id]; }
    /// The sum of the stages' moving averages (the GPU's frame, when the stages are its passes).
    [[nodiscard]] double stage_mean_total() const;

    /// Empties every series; the stages keep their ids.
    void clear();

private:
    std::size_t window_;
    RollingSeries frame_;
    std::vector<std::string> names_;
    std::vector<RollingSeries> stages_;
};

} // namespace ergo::perf
