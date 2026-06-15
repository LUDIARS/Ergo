#pragma once

/// ergo::math — conversion helpers between ergo::math::Vec and ergo::vector types.
///
/// Include this header explicitly when you need interop between the two
/// modules. Both ergo/vector/vector.h and ergo/math/vec.h must be visible.
///
/// Spec: spec/module/math.md

#include "ergo/math/vec.h"
#include "ergo/vector/vector.h"

namespace ergo::math {

/// Convert ergo::vector::Vec2 → ergo::math::Vec2f
[[nodiscard]] inline Vec2f from_ergo_vector(const ergo::vector::Vec2& v) noexcept {
    Vec2f r;
    r.data[0] = v.x;
    r.data[1] = v.y;
    return r;
}

/// Convert ergo::math::Vec2f → ergo::vector::Vec2
[[nodiscard]] inline ergo::vector::Vec2 to_ergo_vector(const Vec2f& v) noexcept {
    return {v.data[0], v.data[1]};
}

/// Convert ergo::vector::Vec3 → ergo::math::Vec3f
[[nodiscard]] inline Vec3f from_ergo_vector3(const ergo::vector::Vec3& v) noexcept {
    Vec3f r;
    r.data[0] = v.x;
    r.data[1] = v.y;
    r.data[2] = v.z;
    return r;
}

/// Convert ergo::math::Vec3f → ergo::vector::Vec3
[[nodiscard]] inline ergo::vector::Vec3 to_ergo_vector3(const Vec3f& v) noexcept {
    return {v.data[0], v.data[1], v.data[2]};
}

}  // namespace ergo::math
