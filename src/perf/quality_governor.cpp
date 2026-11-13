#include "ergo/perf/quality_governor.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ergo::perf {

QualityGovernor::QualityGovernor(QualityLadder ladder, GovernorConfig config)
    : ladder_(std::move(ladder)),
      config_(config),
      window_(std::max<std::size_t>(config.window, 1)),
      settle_left_(config.settle),  // the first frames (pipelines, caches warming up) do not count
      upgrade_hold_(config.upgrade_hold) {}

void QualityGovernor::set_step(int step) {
    step_ = std::clamp(step, 0, ladder_.step_count());
    moved();
    upgraded_ = false;
}

void QualityGovernor::pin(int step) {
    set_step(step);
    pinned_ = true;
}

int QualityGovernor::level(std::string_view name) const {
    const auto stage = ladder_.find(name);
    if (!stage) return -1;
    return ladder_.levels_at(step_)[*stage];
}

double QualityGovernor::statistic() const {
    return window_.full() ? window_.percentile(config_.percentile) : 0.0;
}

void QualityGovernor::moved() {
    window_.clear();
    over_ = 0;
    under_ = 0;
    settle_left_ = config_.settle;
}

GovernorChange QualityGovernor::observe(double frame_cost_ms) {
    if (!std::isfinite(frame_cost_ms) || frame_cost_ms <= 0.0) return GovernorChange::none;
    ++since_upgrade_;
    if (settle_left_ > 0) {
        --settle_left_;
        return GovernorChange::none;
    }
    window_.add(frame_cost_ms);
    if (pinned_ || !window_.full()) return GovernorChange::none;

    const double stat = window_.percentile(config_.percentile);
    if (stat > config_.target_ms * config_.degrade_above) {
        ++over_;
        under_ = 0;
    } else if (stat < config_.target_ms * config_.upgrade_below) {
        ++under_;
        over_ = 0;
    } else {
        over_ = 0;
        under_ = 0;
    }

    if (over_ >= config_.degrade_hold && step_ < ladder_.step_count()) {
        // The last upgrade did not hold: wait twice as long before the next one.
        if (upgraded_ && since_upgrade_ <= upgrade_hold_ + config_.window + config_.settle) {
            upgrade_hold_ = std::min(upgrade_hold_ * 2, std::max(config_.max_upgrade_hold, config_.upgrade_hold));
        }
        upgraded_ = false;
        ++step_;
        moved();
        return GovernorChange::lighter;
    }
    if (under_ >= upgrade_hold_ && step_ > 0) {
        --step_;
        moved();
        upgraded_ = true;
        since_upgrade_ = 0;
        return GovernorChange::heavier;
    }
    return GovernorChange::none;
}

} // namespace ergo::perf
