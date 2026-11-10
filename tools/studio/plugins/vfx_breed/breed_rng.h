#pragma once
/// Seeded deterministic RNG (mulberry32), bit-identical to vfx-breed/rng.ts:
/// the same seed yields the same sequence in the TS and the C++ Studio.
#include <cstdint>

namespace ergo::studio::plugins::vfx_breed {

class BreedRng {
public:
    /// floor(seed) as uint32; 0 becomes 0x9e3779b9 (as `>>> 0 || golden`).
    explicit BreedRng(double seed);

    double   next();        ///< uniform [0, 1)
    double   gaussian();    ///< Box–Muller standard normal
    uint32_t below(uint32_t n);   ///< uniform integer [0, n)

private:
    uint32_t s_;
};

/// A fresh random seed for requests that did not pass one.
uint32_t fresh_seed();

/// JavaScript Math.round (halves round towards +infinity).
double js_round(double v);

} // namespace ergo::studio::plugins::vfx_breed
