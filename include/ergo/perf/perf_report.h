#pragma once

/// ergo_perf - one log line for a frame's statistics.
///
/// Full spec: spec/feature/module/perf.md

#include <string>

#include "ergo/perf/frame_stats.h"
#include "ergo/perf/rolling_series.h"

namespace ergo::perf {

/// "17.70 ms (p50 17.52, p95 18.90, p99 21.04, worst 30.11)".
[[nodiscard]] std::string format_summary(const SeriesSummary& s);

/// "frame 17.70 ms (p50 ..) | shadow 0.10, geometry 2.31, ..." - the stages by
/// their moving averages, in the order they were first named.
[[nodiscard]] std::string format_frame_report(const FrameStats& stats);

} // namespace ergo::perf
