#pragma once

/// ergo_perf - holds a frame budget by moving along a QualityLadder.
///
/// Each frame the caller hands in what the frame cost (the larger of the GPU's
/// time and the CPU's busy time; not the time spent waiting for vsync or a
/// pacing sleep, which would hide the headroom). The governor keeps a window
/// of those costs and judges a high percentile of it, so a lone slow frame
/// (a hitch, a load) does not count. It lightens one step when the window
/// stays over the budget for `degrade_hold` frames, and goes back one step only
/// after `upgrade_hold` frames well under it. After every change it waits
/// (`settle`) and refills the window before judging again; an upgrade that is
/// soon taken back doubles the next upgrade's hold (no flip-flopping).
///
/// Full spec: spec/feature/module/perf.md

#include <cstddef>
#include <string_view>
#include <vector>

#include "ergo/perf/quality_ladder.h"
#include "ergo/perf/rolling_series.h"

namespace ergo::perf {

struct GovernorConfig {
    double target_ms = 1000.0 / 60.0;  ///< the frame budget
    double degrade_above = 1.05;       ///< lighten when the statistic stays above target x this
    double upgrade_below = 0.75;       ///< go heavier only when it stays under target x this
    double percentile = 0.90;          ///< the window's statistic (the slowest 10% are spikes)
    std::size_t window = 60;           ///< frames judged together
    std::size_t degrade_hold = 20;     ///< frames in a row over the line before a lighter step
    std::size_t upgrade_hold = 180;    ///< frames in a row under the line before a heavier step
    std::size_t settle = 20;           ///< frames ignored after a change (pipelines warm up)
    std::size_t max_upgrade_hold = 3600;
};

enum class GovernorChange { none, lighter, heavier };

class QualityGovernor {
public:
    explicit QualityGovernor(QualityLadder ladder, GovernorConfig config = {});

    /// One frame's cost (ms). Non-finite or non-positive costs are ignored.
    GovernorChange observe(double frame_cost_ms);

    /// Start from `step` (an estimate); the governor goes on adapting from there.
    void set_step(int step);
    /// Hold `step` whatever the frames cost (a player's fixed choice).
    void pin(int step);
    void unpin() { pinned_ = false; }
    [[nodiscard]] bool pinned() const { return pinned_; }

    [[nodiscard]] int step() const { return step_; }
    [[nodiscard]] int step_count() const { return ladder_.step_count(); }
    [[nodiscard]] std::vector<int> levels() const { return ladder_.levels_at(step_); }
    /// The level of stage `name` at the current step (-1: no such stage).
    [[nodiscard]] int level(std::string_view name) const;
    /// The window's statistic now (0 until it has filled).
    [[nodiscard]] double statistic() const;
    [[nodiscard]] std::size_t upgrade_hold() const { return upgrade_hold_; }

    [[nodiscard]] const QualityLadder& ladder() const { return ladder_; }
    [[nodiscard]] const GovernorConfig& config() const { return config_; }

private:
    void moved();

    QualityLadder ladder_;
    GovernorConfig config_;
    RollingSeries window_;
    int step_ = 0;
    bool pinned_ = false;
    std::size_t over_ = 0;
    std::size_t under_ = 0;
    std::size_t settle_left_ = 0;
    std::size_t upgrade_hold_;
    std::size_t since_upgrade_ = 0;
    bool upgraded_ = false;
};

} // namespace ergo::perf
