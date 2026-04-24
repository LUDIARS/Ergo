#pragma once
//
// Tiny benchmark harness for Ergo modules.
//
// Deliberately avoids Google Benchmark: benchmarks live alongside tests in
// the same CMake tree and should build with zero external deps. The API is
// a single function `run_bench(name, iterations, fn)` that times `fn` with
// `<chrono>::steady_clock`, prints a one-line summary, and returns the
// measured nanoseconds so callers can aggregate a Markdown table.
//
// Usage:
//   int main() {
//       ergo_bench::run_bench("curve.evaluate", 100000, [&]{ curve.evaluate(...); });
//       return 0;
//   }
//
// The harness warms up for 10% of the requested iterations (not timed)
// before measuring, and reports the best of three runs so noise from the
// OS scheduler is reduced.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace ergo_bench {

struct Result {
    std::string name;
    uint64_t    iterations = 0;
    double      best_ns_per_op = 0.0;
    double      mean_ns_per_op = 0.0;
    double      worst_ns_per_op = 0.0;
};

template <typename F>
Result run_bench(const char* name, uint64_t iterations, F&& body) {
    using clock = std::chrono::steady_clock;
    const uint64_t warmup = std::max<uint64_t>(1, iterations / 10);
    for (uint64_t i = 0; i < warmup; ++i) body();

    std::vector<double> run_ns;
    run_ns.reserve(3);
    for (int r = 0; r < 3; ++r) {
        const auto t0 = clock::now();
        for (uint64_t i = 0; i < iterations; ++i) body();
        const auto t1 = clock::now();
        const double ns = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        run_ns.push_back(ns / static_cast<double>(iterations));
    }

    Result out;
    out.name       = name;
    out.iterations = iterations;
    out.best_ns_per_op  = *std::min_element(run_ns.begin(), run_ns.end());
    out.worst_ns_per_op = *std::max_element(run_ns.begin(), run_ns.end());
    out.mean_ns_per_op  = (run_ns[0] + run_ns[1] + run_ns[2]) / 3.0;

    std::printf("[bench] %-40s  N=%-8llu  best=%9.2f ns  mean=%9.2f ns  worst=%9.2f ns\n",
                out.name.c_str(),
                static_cast<unsigned long long>(out.iterations),
                out.best_ns_per_op, out.mean_ns_per_op, out.worst_ns_per_op);
    return out;
}

/// Convenience: run a benchmark that processes N items per call, and
/// reports ns-per-item instead of ns-per-call. `items_per_call` is baked
/// into the denominator.
template <typename F>
Result run_bench_per_item(const char* name, uint64_t iterations,
                          uint64_t items_per_call, F&& body)
{
    Result r = run_bench(name, iterations, std::forward<F>(body));
    if (items_per_call == 0) items_per_call = 1;
    r.best_ns_per_op  /= static_cast<double>(items_per_call);
    r.mean_ns_per_op  /= static_cast<double>(items_per_call);
    r.worst_ns_per_op /= static_cast<double>(items_per_call);
    std::printf("[bench] %-40s  per-item: best=%7.2f ns  mean=%7.2f ns\n",
                r.name.c_str(), r.best_ns_per_op, r.mean_ns_per_op);
    return r;
}

/// Print a Markdown summary of multiple results. Useful for piping bench
/// output into a regression-tracking file.
inline void print_markdown(const std::vector<Result>& results) {
    std::printf("\n| benchmark | N | best (ns) | mean (ns) | worst (ns) |\n");
    std::printf("|---|---:|---:|---:|---:|\n");
    for (const auto& r : results) {
        std::printf("| %s | %llu | %.2f | %.2f | %.2f |\n",
                    r.name.c_str(),
                    static_cast<unsigned long long>(r.iterations),
                    r.best_ns_per_op, r.mean_ns_per_op, r.worst_ns_per_op);
    }
    std::printf("\n");
}

} // namespace ergo_bench
