#include "breed_rng.h"

#include <cmath>
#include <random>

namespace ergo::studio::plugins::vfx_breed {

BreedRng::BreedRng(double seed) {
    // JS: Math.floor(seed) >>> 0  (ToUint32: modulo 2^32 of the integer part)
    const double f = std::floor(std::isfinite(seed) ? seed : 0);
    const double m = std::fmod(f, 4294967296.0);
    const uint32_t u = static_cast<uint32_t>(m < 0 ? m + 4294967296.0 : m);
    s_ = u != 0 ? u : 0x9e3779b9u;
}

double BreedRng::next() {
    s_ += 0x6d2b79f5u;
    uint32_t t = s_;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

double BreedRng::gaussian() {
    const double u = 1 - next();   // (0, 1]: keeps log() finite
    const double v = next();
    return std::sqrt(-2 * std::log(u)) * std::cos(2 * 3.14159265358979323846 * v);
}

uint32_t BreedRng::below(uint32_t n) { return static_cast<uint32_t>(std::floor(next() * n)); }

uint32_t fresh_seed() {
    static thread_local std::mt19937 gen{std::random_device{}()};
    return static_cast<uint32_t>(gen());
}

double js_round(double v) { return std::floor(v + 0.5); }

} // namespace ergo::studio::plugins::vfx_breed
