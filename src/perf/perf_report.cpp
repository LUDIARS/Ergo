#include "ergo/perf/perf_report.h"

#include <cstdio>

namespace ergo::perf {

std::string format_summary(const SeriesSummary& s) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%.2f ms (p50 %.2f, p95 %.2f, p99 %.2f, worst %.2f)", s.mean, s.p50, s.p95,
                  s.p99, s.worst);
    return buf;
}

std::string format_frame_report(const FrameStats& stats) {
    std::string line = "frame " + format_summary(stats.frame().summary());
    if (stats.stage_count() == 0) return line;
    line += " |";
    char buf[96];
    for (std::size_t i = 0; i < stats.stage_count(); ++i) {
        std::snprintf(buf, sizeof(buf), "%s %s %.2f", i == 0 ? "" : ",", stats.stage_name(i).c_str(),
                      stats.stage_series(i).mean());
        line += buf;
    }
    return line;
}

} // namespace ergo::perf
