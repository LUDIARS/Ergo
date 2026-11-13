#include "gtest/gtest.h"

#include <cmath>
#include <limits>
#include <string>

#include "ergo/perf/frame_stats.h"
#include "ergo/perf/perf_report.h"
#include "ergo/perf/rolling_series.h"

using namespace ergo::perf;

TEST(RollingSeries, EmptyReadsZero) {
    RollingSeries s(8);
    EXPECT_EQ(s.size(), 0u);
    EXPECT_DOUBLE_EQ(s.mean(), 0.0);
    EXPECT_DOUBLE_EQ(s.percentile(0.95), 0.0);
    EXPECT_DOUBLE_EQ(s.worst(), 0.0);
}

TEST(RollingSeries, NearestRankPercentilesOverOneToHundred) {
    RollingSeries s(100);
    for (int i = 100; i >= 1; --i) s.add(static_cast<double>(i));  // order must not matter
    EXPECT_TRUE(s.full());
    EXPECT_DOUBLE_EQ(s.percentile(0.50), 50.0);
    EXPECT_DOUBLE_EQ(s.percentile(0.95), 95.0);
    EXPECT_DOUBLE_EQ(s.percentile(0.99), 99.0);
    EXPECT_DOUBLE_EQ(s.percentile(1.0), 100.0);
    EXPECT_DOUBLE_EQ(s.percentile(0.0), 1.0);
    EXPECT_DOUBLE_EQ(s.mean(), 50.5);
    EXPECT_DOUBLE_EQ(s.worst(), 100.0);
    EXPECT_DOUBLE_EQ(s.last(), 1.0);
}

TEST(RollingSeries, WindowDropsTheOldest) {
    RollingSeries s(4);
    for (int i = 1; i <= 6; ++i) s.add(static_cast<double>(i));  // keeps 3, 4, 5, 6
    EXPECT_EQ(s.size(), 4u);
    EXPECT_DOUBLE_EQ(s.mean(), 4.5);
    EXPECT_DOUBLE_EQ(s.worst(), 6.0);
    EXPECT_DOUBLE_EQ(s.percentile(0.0), 3.0);
}

TEST(RollingSeries, IgnoresUnmeasuredSamples) {
    RollingSeries s(4);
    s.add(std::numeric_limits<double>::quiet_NaN());
    s.add(std::numeric_limits<double>::infinity());
    s.add(-1.0);
    s.add(2.0);
    EXPECT_EQ(s.size(), 1u);
    EXPECT_DOUBLE_EQ(s.mean(), 2.0);
}

TEST(RollingSeries, SummaryCarriesEveryStatistic) {
    RollingSeries s(100);
    for (int i = 1; i <= 100; ++i) s.add(static_cast<double>(i));
    const SeriesSummary sum = s.summary();
    EXPECT_EQ(sum.count, 100u);
    EXPECT_DOUBLE_EQ(sum.p95, 95.0);
    EXPECT_DOUBLE_EQ(sum.p99, 99.0);
    EXPECT_DOUBLE_EQ(sum.worst, 100.0);
    EXPECT_DOUBLE_EQ(sum.last, 100.0);
}

TEST(FrameStats, StagesKeepTheirIdsInFirstNamedOrder) {
    FrameStats stats(16);
    EXPECT_EQ(stats.stage("lighting"), 0u);
    EXPECT_EQ(stats.stage("fog"), 1u);
    EXPECT_EQ(stats.stage("lighting"), 0u);
    EXPECT_EQ(stats.stage_count(), 2u);
    EXPECT_EQ(stats.stage_name(1), "fog");
}

TEST(FrameStats, StageMeansAddUpToTheGpuFrame) {
    FrameStats stats(16);
    for (int i = 0; i < 10; ++i) {
        stats.add_frame(17.7);
        stats.add_stage("lighting", 5.4);
        stats.add_stage("fog", 3.7);
        stats.add_stage("geometry", 2.4 + (i % 2) * 0.4);
    }
    EXPECT_NEAR(stats.stage_mean_total(), 5.4 + 3.7 + 2.6, 1e-9);
    EXPECT_NEAR(stats.frame().mean(), 17.7, 1e-9);
    stats.clear();
    EXPECT_EQ(stats.frame().size(), 0u);
    EXPECT_EQ(stats.stage_count(), 3u);  // ids survive a clear
    EXPECT_EQ(stats.stage_series(0).size(), 0u);
}

TEST(PerfReport, OneLineWithTheFrameAndEachStage) {
    FrameStats stats(8);
    stats.add_frame(16.0);
    stats.add_stage("lighting", 5.5);
    stats.add_stage("fog", 3.25);
    const std::string line = format_frame_report(stats);
    EXPECT_EQ(line.rfind("frame 16.00 ms (p50 16.00, p95 16.00, p99 16.00, worst 16.00)", 0), 0u);
    EXPECT_TRUE(line.find("| lighting 5.50, fog 3.25") != std::string::npos);
}
