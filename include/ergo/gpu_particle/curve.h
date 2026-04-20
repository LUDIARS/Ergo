#pragma once

#include "ergo/gpu_particle/types.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ergo::gpu_particle {

// ---------------------------------------------------------------------------
// Curve — piecewise-linear animation curve with CPU evaluation and a fixed
// sample count for GPU upload.
//
// Shuriken-style curves are typically authored over the 0..1 domain
// (normalized particle lifetime OR normalized emitter time). Keys outside
// [0,1] are allowed and clamped at evaluation time.
//
// The baked representation is a fixed-size float array so every curve
// occupies the same slot in the per-emitter UBO — no indirection,
// straightforward to sample on the GPU (`texelFetch` / `texture` over a
// 1D buffer).
// ---------------------------------------------------------------------------

struct CurveKey {
    float time  = 0.f;   // typically 0..1
    float value = 0.f;
};

class Curve {
public:
    /// Number of samples in the baked GPU-friendly representation.
    /// 32 samples at LERP is usually visually indistinguishable from
    /// spline interpolation for size/color curves at typical durations.
    static constexpr uint32_t kSampleCount = 32;
    using BakedArray = std::array<float, kSampleCount>;

    Curve() = default;

    // ---- Construction helpers ----
    static Curve constant(float v);
    static Curve linear(float v0, float v1);
    static Curve ease_in_out(float v0, float v1);

    /// Append a key. Keys are kept sorted by time; passing an earlier
    /// time than the last-added key triggers an insertion sort.
    Curve& add_key(float time, float value);
    Curve& clear();

    /// CPU evaluation. `time` is clamped into the range of added keys.
    float evaluate(float time) const;

    /// Write the curve into a dense sample buffer for GPU upload.
    /// The samples are evenly spaced over the [0,1] domain.
    void bake(BakedArray& out) const;

    /// Raw key access (for serialization / diagnostics).
    const std::vector<CurveKey>& keys() const { return keys_; }

    bool empty() const { return keys_.empty(); }

private:
    std::vector<CurveKey> keys_;
};

// ---------------------------------------------------------------------------
// MinMaxCurve — Unity's Particle System emulation for per-property inputs
// that can be authored as a constant, two constants (random between),
// a single curve, or two curves (random between curves).
// ---------------------------------------------------------------------------

struct MinMaxCurve {
    enum class Mode : uint8_t {
        Constant       = 0,
        TwoConstants   = 1,
        Curve          = 2,
        TwoCurves      = 3,
    };

    Mode  mode            = Mode::Constant;
    float constant_min    = 0.f;
    float constant_max    = 0.f;
    Curve curve_min;
    Curve curve_max;

    static MinMaxCurve constant(float v) {
        MinMaxCurve c;
        c.mode = Mode::Constant;
        c.constant_min = c.constant_max = v;
        return c;
    }

    static MinMaxCurve two_constants(float lo, float hi) {
        MinMaxCurve c;
        c.mode = Mode::TwoConstants;
        c.constant_min = lo;
        c.constant_max = hi;
        return c;
    }

    /// `time` is normalized (0..1). `rand01` is a uniform random number
    /// used to interpolate between the lower/upper bound when the mode
    /// has a range.
    float evaluate(float time, float rand01) const;
};

} // namespace ergo::gpu_particle
