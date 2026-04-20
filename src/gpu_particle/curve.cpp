#include "ergo/gpu_particle/curve.h"

#include <algorithm>
#include <cassert>

namespace ergo::gpu_particle {

// ---- Curve --------------------------------------------------------------

Curve Curve::constant(float v) {
    Curve c;
    c.add_key(0.0f, v);
    c.add_key(1.0f, v);
    return c;
}

Curve Curve::linear(float v0, float v1) {
    Curve c;
    c.add_key(0.0f, v0);
    c.add_key(1.0f, v1);
    return c;
}

Curve Curve::ease_in_out(float v0, float v1) {
    // Approximate a smooth-step via extra midpoints. Not a spline but
    // visually acceptable at kSampleCount = 32.
    Curve c;
    c.add_key(0.00f, v0);
    c.add_key(0.25f, v0 + (v1 - v0) * 0.07f);
    c.add_key(0.50f, v0 + (v1 - v0) * 0.50f);
    c.add_key(0.75f, v0 + (v1 - v0) * 0.93f);
    c.add_key(1.00f, v1);
    return c;
}

Curve& Curve::add_key(float time, float value) {
    CurveKey k{time, value};
    auto it = std::upper_bound(keys_.begin(), keys_.end(), k,
        [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
    keys_.insert(it, k);
    return *this;
}

Curve& Curve::clear() {
    keys_.clear();
    return *this;
}

float Curve::evaluate(float time) const {
    if (keys_.empty()) return 0.0f;
    if (keys_.size() == 1) return keys_[0].value;
    if (time <= keys_.front().time) return keys_.front().value;
    if (time >= keys_.back().time)  return keys_.back().value;

    // Binary search for the interval.
    size_t lo = 0, hi = keys_.size() - 1;
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (keys_[mid].time <= time) lo = mid; else hi = mid;
    }
    const CurveKey& a = keys_[lo];
    const CurveKey& b = keys_[hi];
    const float span = b.time - a.time;
    if (span <= 0.0f) return a.value;
    const float t = (time - a.time) / span;
    return a.value + (b.value - a.value) * t;
}

void Curve::bake(BakedArray& out) const {
    for (uint32_t i = 0; i < kSampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleCount - 1);
        out[i] = evaluate(t);
    }
}

// ---- MinMaxCurve --------------------------------------------------------

float MinMaxCurve::evaluate(float time, float rand01) const {
    const float r = std::clamp(rand01, 0.0f, 1.0f);
    switch (mode) {
        case Mode::Constant:
            return constant_min;
        case Mode::TwoConstants:
            return constant_min + (constant_max - constant_min) * r;
        case Mode::Curve:
            return curve_min.evaluate(time);
        case Mode::TwoCurves: {
            const float a = curve_min.evaluate(time);
            const float b = curve_max.evaluate(time);
            return a + (b - a) * r;
        }
    }
    return constant_min;
}

} // namespace ergo::gpu_particle
