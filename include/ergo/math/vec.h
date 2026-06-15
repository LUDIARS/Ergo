#pragma once

/// ergo::math::Vec — generic constexpr vector template.
///
/// Provides: +,-,*,/ (scalar and component-wise), +=,-=,*=,/=,
///           dot, length_sq, length, normalized, lerp, approx_eq.
/// x()/y()/z()/w() accessors are enabled by if constexpr for N>=1..4.
/// Type aliases: Vec2f/Vec3f/Vec4f (float), Vec2d/Vec3d/Vec4d (double).
/// Conversion helpers to/from ergo::vector::Vec2 and Vec3.
///
/// Spec: spec/module/math.md

#include "ergo/math/concepts.h"

#include <algorithm>   // std::clamp
#include <cmath>       // std::sqrt
#include <cstddef>

namespace ergo::math {

// ---------------------------------------------------------------------------
// Vec<N, T> — N-component vector
// ---------------------------------------------------------------------------

template <std::size_t N, Scalar T>
struct Vec {
    T data[N]{};

    // ---- element access -------------------------------------------------------
    constexpr T& operator[](std::size_t i) noexcept { return data[i]; }
    constexpr const T& operator[](std::size_t i) const noexcept { return data[i]; }

    // ---- named accessors (enabled by if constexpr) ---------------------------
    constexpr T& x() noexcept { if constexpr (N >= 1) return data[0]; }
    constexpr const T& x() const noexcept { if constexpr (N >= 1) return data[0]; }

    constexpr T& y() noexcept { if constexpr (N >= 2) return data[1]; }
    constexpr const T& y() const noexcept { if constexpr (N >= 2) return data[1]; }

    constexpr T& z() noexcept { if constexpr (N >= 3) return data[2]; }
    constexpr const T& z() const noexcept { if constexpr (N >= 3) return data[2]; }

    constexpr T& w() noexcept { if constexpr (N >= 4) return data[3]; }
    constexpr const T& w() const noexcept { if constexpr (N >= 4) return data[3]; }

    // ---- unary minus ---------------------------------------------------------
    constexpr Vec operator-() const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = -data[i];
        return r;
    }

    // ---- component-wise binary -----------------------------------------------
    constexpr Vec operator+(const Vec& rhs) const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] + rhs.data[i];
        return r;
    }
    constexpr Vec operator-(const Vec& rhs) const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] - rhs.data[i];
        return r;
    }
    constexpr Vec operator*(const Vec& rhs) const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] * rhs.data[i];
        return r;
    }
    constexpr Vec operator/(const Vec& rhs) const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] / rhs.data[i];
        return r;
    }

    // ---- scalar binary -------------------------------------------------------
    constexpr Vec operator*(T s) const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] * s;
        return r;
    }
    constexpr Vec operator/(T s) const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] / s;
        return r;
    }

    // ---- compound assignment -------------------------------------------------
    constexpr Vec& operator+=(const Vec& rhs) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] += rhs.data[i];
        return *this;
    }
    constexpr Vec& operator-=(const Vec& rhs) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] -= rhs.data[i];
        return *this;
    }
    constexpr Vec& operator*=(T s) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] *= s;
        return *this;
    }
    constexpr Vec& operator/=(T s) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] /= s;
        return *this;
    }

    // ---- dot product ---------------------------------------------------------
    [[nodiscard]] constexpr T dot(const Vec& rhs) const noexcept {
        T acc{};
        for (std::size_t i = 0; i < N; ++i) acc += data[i] * rhs.data[i];
        return acc;
    }

    // ---- length --------------------------------------------------------------
    [[nodiscard]] constexpr T length_sq() const noexcept {
        return dot(*this);
    }
    [[nodiscard]] T length() const noexcept {
        return std::sqrt(length_sq());
    }

    // ---- normalized ----------------------------------------------------------
    /// Returns a unit vector. If length is near-zero, returns zero vector.
    [[nodiscard]] Vec normalized() const noexcept {
        T len = length();
        if (len < T(1e-12)) return Vec{};
        return *this / len;
    }

    // ---- lerp ----------------------------------------------------------------
    [[nodiscard]] constexpr Vec lerp(const Vec& to, T t) const noexcept {
        Vec r;
        for (std::size_t i = 0; i < N; ++i)
            r.data[i] = data[i] + (to.data[i] - data[i]) * t;
        return r;
    }

    // ---- approximate equality ------------------------------------------------
    [[nodiscard]] constexpr bool approx_eq(const Vec& rhs,
                                           T eps = T(1e-6)) const noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            T diff = data[i] - rhs.data[i];
            if (diff < -eps || diff > eps) return false;
        }
        return true;
    }
};

// ---- scalar * vec (commutative) ------------------------------------------
template <std::size_t N, Scalar T>
constexpr Vec<N, T> operator*(T s, const Vec<N, T>& v) noexcept {
    return v * s;
}

// ---------------------------------------------------------------------------
// Type aliases
// ---------------------------------------------------------------------------

using Vec2f = Vec<2, float>;
using Vec3f = Vec<3, float>;
using Vec4f = Vec<4, float>;
using Vec2d = Vec<2, double>;
using Vec3d = Vec<3, double>;
using Vec4d = Vec<4, double>;

// ---------------------------------------------------------------------------
// Conversion helpers to/from ergo::vector::Vec2 / Vec3
// ---------------------------------------------------------------------------
// Forward-declare the ergo::vector types so we don't pull in the full header
// unless the consumer already included it. The helpers are defined below and
// will cause a compile error only if you actually call them without the header.

}  // namespace ergo::math

// Include ergo::vector only when available (header-include guard trick).
// Consumers that have included ergo/vector/vector.h will get the overloads.
#ifdef ERGO_VECTOR_VECTOR_H  // set by ergo/vector/vector.h if it uses that guard
#include "ergo/math/vec_vector_convert.h"
#endif

// ergo/vector/vector.h uses #pragma once but no custom guard macro, so we
// provide conversion as inline helpers in a separate header that users include
// explicitly when they want cross-module conversion.
// See ergo/math/vec_vector_convert.h
