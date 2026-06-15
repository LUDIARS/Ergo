#pragma once

/// ergo::math::batch — SoA batch math operations with optional SSE2 fast path.
///
/// Storage layout: SoA (Structure of Arrays).
///   float x[], float y[] for 2D vectors, not interleaved Vec2f[].
/// Rationale: SoA enables SIMD without gather/scatter, auto-vectorizes cleanly,
///            and is the natural layout for physics / particle solvers.
///
/// Scalar fallback is always compiled; SSE2 path is activated when:
///   defined(__SSE2__) || defined(_M_X64)   (i.e., all x64 MSVC/GCC/Clang)
/// Compile with -DERGO_MATH_SIMD=0 to force scalar path even on SSE2 hosts.
///
/// All SSE2 results must be bit-for-bit identical to the scalar path for
/// identical inputs (tested in tests/math/test_math.cpp).
///
/// Spec: spec/module/math.md

#include "ergo/math/concepts.h"

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// SIMD availability probe
// ---------------------------------------------------------------------------

#ifndef ERGO_MATH_SIMD
#  if (defined(__SSE2__) || defined(_M_X64))
#    define ERGO_MATH_SIMD 1
#  else
#    define ERGO_MATH_SIMD 0
#  endif
#endif

#if ERGO_MATH_SIMD
#  include <immintrin.h>
#endif

namespace ergo::math::batch {

// ---------------------------------------------------------------------------
// add_f32  —  out[i] = a[i] + b[i]
// ---------------------------------------------------------------------------

inline void add_f32(const float* __restrict a,
                    const float* __restrict b,
                    float* __restrict out,
                    std::size_t n) noexcept {
#if ERGO_MATH_SIMD
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        _mm_storeu_ps(out + i, _mm_add_ps(va, vb));
    }
    for (; i < n; ++i) out[i] = a[i] + b[i];
#else
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] + b[i];
#endif
}

// ---------------------------------------------------------------------------
// scale_f32  —  out[i] = a[i] * s
// ---------------------------------------------------------------------------

inline void scale_f32(const float* __restrict a,
                      float s,
                      float* __restrict out,
                      std::size_t n) noexcept {
#if ERGO_MATH_SIMD
    __m128 vs = _mm_set1_ps(s);
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        _mm_storeu_ps(out + i, _mm_mul_ps(va, vs));
    }
    for (; i < n; ++i) out[i] = a[i] * s;
#else
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] * s;
#endif
}

// ---------------------------------------------------------------------------
// madd_f32  —  a[i] += s * b[i]   (multiply-add, in-place on a)
// ---------------------------------------------------------------------------

inline void madd_f32(float* __restrict a,
                     float s,
                     const float* __restrict b,
                     std::size_t n) noexcept {
#if ERGO_MATH_SIMD
    __m128 vs = _mm_set1_ps(s);
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        _mm_storeu_ps(a + i, _mm_add_ps(va, _mm_mul_ps(vs, vb)));
    }
    for (; i < n; ++i) a[i] += s * b[i];
#else
    for (std::size_t i = 0; i < n; ++i) a[i] += s * b[i];
#endif
}

// ---------------------------------------------------------------------------
// dot_f32  —  returns sum of a[i]*b[i] for i in [0,n)
// ---------------------------------------------------------------------------

inline float dot_f32(const float* __restrict a,
                     const float* __restrict b,
                     std::size_t n) noexcept {
#if ERGO_MATH_SIMD
    __m128 acc = _mm_setzero_ps();
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        acc = _mm_add_ps(acc, _mm_mul_ps(va, vb));
    }
    // horizontal sum of acc
    __m128 shuf = _mm_shuffle_ps(acc, acc, 0b10110001); // swap pairs
    __m128 sums = _mm_add_ps(acc, shuf);
    shuf = _mm_movehl_ps(shuf, sums);                   // move high
    sums = _mm_add_ss(sums, shuf);
    float result = _mm_cvtss_f32(sums);
    for (; i < n; ++i) result += a[i] * b[i];
    return result;
#else
    float acc = 0.0f;
    for (std::size_t i = 0; i < n; ++i) acc += a[i] * b[i];
    return acc;
#endif
}

// ---------------------------------------------------------------------------
// 2D SoA convenience wrappers
// (x[],y[] layout — caller owns/manages the arrays)
// ---------------------------------------------------------------------------

/// Add two SoA Vec2 arrays: ox[i]=ax[i]+bx[i], oy[i]=ay[i]+by[i]
inline void add2_f32(const float* __restrict ax, const float* __restrict ay,
                     const float* __restrict bx, const float* __restrict by,
                     float* __restrict ox,       float* __restrict oy,
                     std::size_t n) noexcept {
    add_f32(ax, bx, ox, n);
    add_f32(ay, by, oy, n);
}

/// Scale SoA Vec2 array: ox[i]=ax[i]*s, oy[i]=ay[i]*s
inline void scale2_f32(const float* __restrict ax, const float* __restrict ay,
                       float s,
                       float* __restrict ox,       float* __restrict oy,
                       std::size_t n) noexcept {
    scale_f32(ax, s, ox, n);
    scale_f32(ay, s, oy, n);
}

/// madd SoA Vec2: ax[i]+=s*bx[i], ay[i]+=s*by[i]
inline void madd2_f32(float* __restrict ax,       float* __restrict ay,
                      float s,
                      const float* __restrict bx, const float* __restrict by,
                      std::size_t n) noexcept {
    madd_f32(ax, s, bx, n);
    madd_f32(ay, s, by, n);
}

/// SoA dot products (per-element): out[i] = ax[i]*bx[i] + ay[i]*by[i]
inline void dot2_f32(const float* __restrict ax, const float* __restrict ay,
                     const float* __restrict bx, const float* __restrict by,
                     float* __restrict out,
                     std::size_t n) noexcept {
#if ERGO_MATH_SIMD
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 vax = _mm_loadu_ps(ax + i);
        __m128 vay = _mm_loadu_ps(ay + i);
        __m128 vbx = _mm_loadu_ps(bx + i);
        __m128 vby = _mm_loadu_ps(by + i);
        __m128 result = _mm_add_ps(_mm_mul_ps(vax, vbx), _mm_mul_ps(vay, vby));
        _mm_storeu_ps(out + i, result);
    }
    for (; i < n; ++i) out[i] = ax[i]*bx[i] + ay[i]*by[i];
#else
    for (std::size_t i = 0; i < n; ++i) out[i] = ax[i]*bx[i] + ay[i]*by[i];
#endif
}

}  // namespace ergo::math::batch
