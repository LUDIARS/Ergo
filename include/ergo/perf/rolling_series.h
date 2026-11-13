#pragma once

/// ergo_perf - a fixed window of samples (frame times, stage times) and its
/// statistics: the moving average, nearest-rank percentiles and the worst.
///
/// Full spec: spec/feature/module/perf.md

#include <cstddef>
#include <vector>

namespace ergo::perf {

/// What a window holds, summed up (milliseconds when it holds times).
struct SeriesSummary {
    std::size_t count = 0;  ///< samples in the window
    double last  = 0.0;     ///< the newest sample
    double mean  = 0.0;     ///< the moving average over the window
    double p50   = 0.0;
    double p95   = 0.0;
    double p99   = 0.0;
    double worst = 0.0;     ///< the largest sample in the window
};

/// The last `capacity` samples, oldest dropped first. Samples that are not
/// finite or are negative are ignored (a stage that was not measured).
class RollingSeries {
public:
    explicit RollingSeries(std::size_t capacity = 240);

    void add(double value);
    void clear();

    [[nodiscard]] std::size_t size() const { return count_; }
    [[nodiscard]] std::size_t capacity() const { return ring_.size(); }
    [[nodiscard]] bool full() const { return count_ == ring_.size(); }

    /// The moving average over the window (0 when empty).
    [[nodiscard]] double mean() const;
    /// Nearest-rank percentile, `p` in [0, 1] (0.95: 95th). 0 when empty.
    [[nodiscard]] double percentile(double p) const;
    [[nodiscard]] double worst() const;
    [[nodiscard]] double last() const;
    [[nodiscard]] SeriesSummary summary() const;

private:
    std::vector<double> ring_;
    std::size_t next_  = 0;
    std::size_t count_ = 0;
    mutable std::vector<double> scratch_;  // sorted copy for percentiles
};

} // namespace ergo::perf
