// Benchmark ergo_gpu_particle::Curve — bake + evaluate hot paths. These
// are called once per emitter (bake) and O(particles * frames) times
// (evaluate) in any curve-driven over-lifetime modifier.

#include "ergo/gpu_particle/curve.h"

#include "bench_common.h"

#include <vector>

using namespace ergo::gpu_particle;

int main() {
    std::vector<ergo_bench::Result> results;

    // 1. Baking a 4-key curve into its baked sample array.
    {
        Curve c;
        c.add_key(0.00f, 0.0f);
        c.add_key(0.25f, 1.0f);
        c.add_key(0.75f, 0.5f);
        c.add_key(1.00f, 0.0f);
        Curve::BakedArray out;
        results.push_back(ergo_bench::run_bench(
            "curve.bake(4 keys)", 10000, [&] { c.bake(out); }));
    }

    // 2. Evaluating a linear-keyed curve at random 0..1 points. Tight
    //    inner loop — representative of shader-side texture sampling
    //    performed CPU-side in the fallback.
    {
        Curve c;
        for (int i = 0; i < 16; ++i) {
            c.add_key(static_cast<float>(i) / 15.0f, (i & 1) ? 1.0f : 0.25f);
        }
        // Pre-roll the xorshift so we don't pollute timing with RNG setup.
        uint32_t state = 0xACEF0DADu;
        auto rand01 = [&] {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
        };
        results.push_back(ergo_bench::run_bench(
            "curve.evaluate(16 keys)", 1000000, [&] {
                volatile float v = c.evaluate(rand01());
                (void)v;
            }));
    }

    // 3. MinMaxCurve::evaluate — used by rate_over_time / start_* ranges.
    {
        MinMaxCurve m = MinMaxCurve::two_constants(2.0f, 8.0f);
        uint32_t state = 0xFEEDBABEu;
        auto rand01 = [&] {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
        };
        results.push_back(ergo_bench::run_bench(
            "minmax.evaluate(constants)", 1000000, [&] {
                volatile float v = m.evaluate(rand01(), rand01());
                (void)v;
            }));
    }

    ergo_bench::print_markdown(results);
    return 0;
}
