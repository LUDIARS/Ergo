#include "ergo/perf/rolling_series.h"

#include <algorithm>
#include <cmath>

namespace ergo::perf {

RollingSeries::RollingSeries(std::size_t capacity)
    : ring_(capacity > 0 ? capacity : 1, 0.0) {}

void RollingSeries::add(double value) {
    if (!std::isfinite(value) || value < 0.0) return;
    ring_[next_] = value;
    next_ = (next_ + 1) % ring_.size();
    if (count_ < ring_.size()) ++count_;
}

void RollingSeries::clear() {
    next_  = 0;
    count_ = 0;
}

double RollingSeries::mean() const {
    if (count_ == 0) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < count_; ++i) sum += ring_[i];
    return sum / static_cast<double>(count_);
}

double RollingSeries::percentile(double p) const {
    if (count_ == 0) return 0.0;
    const double q = std::clamp(p, 0.0, 1.0);
    scratch_.assign(ring_.begin(), ring_.begin() + static_cast<std::ptrdiff_t>(count_));
    // Nearest rank: the smallest sample with at least q of the window at or below it.
    const auto rank = static_cast<std::size_t>(std::ceil(q * static_cast<double>(count_)));
    const std::size_t index = rank == 0 ? 0 : std::min(rank - 1, count_ - 1);
    std::nth_element(scratch_.begin(), scratch_.begin() + static_cast<std::ptrdiff_t>(index), scratch_.end());
    return scratch_[index];
}

double RollingSeries::worst() const {
    if (count_ == 0) return 0.0;
    return *std::max_element(ring_.begin(), ring_.begin() + static_cast<std::ptrdiff_t>(count_));
}

double RollingSeries::last() const {
    if (count_ == 0) return 0.0;
    return ring_[(next_ + ring_.size() - 1) % ring_.size()];
}

SeriesSummary RollingSeries::summary() const {
    SeriesSummary s;
    s.count = count_;
    s.last  = last();
    s.mean  = mean();
    s.p50   = percentile(0.50);
    s.p95   = percentile(0.95);
    s.p99   = percentile(0.99);
    s.worst = worst();
    return s;
}

} // namespace ergo::perf
