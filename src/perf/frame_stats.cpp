#include "ergo/perf/frame_stats.h"

namespace ergo::perf {

FrameStats::FrameStats(std::size_t window) : window_(window), frame_(window) {}

void FrameStats::add_frame(double cpu_ms) { frame_.add(cpu_ms); }

std::size_t FrameStats::stage(std::string_view name) {
    for (std::size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == name) return i;
    }
    names_.emplace_back(name);
    stages_.emplace_back(window_);
    return names_.size() - 1;
}

void FrameStats::add_stage(std::size_t id, double ms) {
    if (id < stages_.size()) stages_[id].add(ms);
}

double FrameStats::stage_mean_total() const {
    double total = 0.0;
    for (const RollingSeries& s : stages_) total += s.mean();
    return total;
}

void FrameStats::clear() {
    frame_.clear();
    for (RollingSeries& s : stages_) s.clear();
}

} // namespace ergo::perf
