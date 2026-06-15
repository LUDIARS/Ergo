#pragma once

/// ergo::math::concepts — C++20 concept definitions for the math module.
///
/// Scalar: any floating-point type.
/// Arithmetic: any floating-point or integer type.
///
/// Spec: spec/module/math.md

#include <concepts>
#include <type_traits>

namespace ergo::math {

/// Scalar: floating-point types only (float, double, long double).
template <class T>
concept Scalar = std::floating_point<T>;

/// Arithmetic: floating-point or integral types.
template <class T>
concept Arithmetic = std::floating_point<T> || std::integral<T>;

}  // namespace ergo::math
