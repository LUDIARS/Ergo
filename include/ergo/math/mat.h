#pragma once

/// ergo::math::Mat — generic row-major matrix template.
///
/// Provides: operator[], multiply (Mat*Mat, Mat*Vec), transpose.
/// Special helpers:
///   Mat2: rotation, determinant, inverse.
///   Mat3: 2D affine/homography (apply_point with homogeneous coords, inverse).
///   Mat4: translation, scale, rotation (X/Y/Z axes), TRS compose,
///         perspective, ortho, inverse (cofactor expansion).
///
/// Row-major storage: element(r, c) = data[r * C + c].
///
/// Spec: spec/module/math.md

#include "ergo/math/concepts.h"
#include "ergo/math/vec.h"

#include <cmath>
#include <cstddef>

namespace ergo::math {

// ---------------------------------------------------------------------------
// Mat<R, C, T>  — R rows, C columns, row-major
// ---------------------------------------------------------------------------

template <std::size_t R, std::size_t C, Scalar T>
struct Mat {
    T data[R * C]{};

    // ---- element access (row pointer) ----------------------------------------
    /// Returns a pointer to row r; use m[r][c] for element access.
    constexpr T* operator[](std::size_t r) noexcept {
        return &data[r * C];
    }
    constexpr const T* operator[](std::size_t r) const noexcept {
        return &data[r * C];
    }

    // ---- identity construction -----------------------------------------------
    /// Returns the identity matrix (works only for square matrices).
    [[nodiscard]] static constexpr Mat identity() noexcept {
        static_assert(R == C, "identity() requires a square matrix");
        Mat m{};
        for (std::size_t i = 0; i < R; ++i) m.data[i * C + i] = T(1);
        return m;
    }

    // ---- transpose -----------------------------------------------------------
    [[nodiscard]] constexpr Mat<C, R, T> transposed() const noexcept {
        Mat<C, R, T> r{};
        for (std::size_t i = 0; i < R; ++i)
            for (std::size_t j = 0; j < C; ++j)
                r[j][i] = (*this)[i][j];
        return r;
    }

    // ---- matrix multiplication -----------------------------------------------
    template <std::size_t K>
    [[nodiscard]] constexpr Mat<R, K, T>
    operator*(const Mat<C, K, T>& rhs) const noexcept {
        Mat<R, K, T> out{};
        for (std::size_t i = 0; i < R; ++i)
            for (std::size_t k = 0; k < C; ++k)
                for (std::size_t j = 0; j < K; ++j)
                    out[i][j] += (*this)[i][k] * rhs[k][j];
        return out;
    }

    // ---- matrix * column vector ----------------------------------------------
    [[nodiscard]] constexpr Vec<R, T>
    operator*(const Vec<C, T>& v) const noexcept {
        Vec<R, T> out{};
        for (std::size_t i = 0; i < R; ++i)
            for (std::size_t j = 0; j < C; ++j)
                out.data[i] += (*this)[i][j] * v.data[j];
        return out;
    }
};

// ---------------------------------------------------------------------------
// Mat2 helpers (2×2)
// ---------------------------------------------------------------------------

using Mat2f = Mat<2, 2, float>;
using Mat2d = Mat<2, 2, double>;

/// Build a 2×2 rotation matrix for angle theta (radians), counter-clockwise.
template <Scalar T>
[[nodiscard]] inline Mat<2, 2, T> mat2_rotation(T theta) noexcept {
    Mat<2, 2, T> m{};
    T c = std::cos(theta);
    T s = std::sin(theta);
    m[0][0] =  c;  m[0][1] = -s;
    m[1][0] =  s;  m[1][1] =  c;
    return m;
}

/// Determinant of a 2×2 matrix.
template <Scalar T>
[[nodiscard]] constexpr T mat2_det(const Mat<2, 2, T>& m) noexcept {
    return m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

/// Inverse of a 2×2 matrix (closed form). Returns zero matrix if singular.
template <Scalar T>
[[nodiscard]] inline Mat<2, 2, T> mat2_inverse(const Mat<2, 2, T>& m) noexcept {
    T det = mat2_det(m);
    if (det == T(0)) return {};
    T inv = T(1) / det;
    Mat<2, 2, T> r{};
    r[0][0] =  m[1][1] * inv;
    r[0][1] = -m[0][1] * inv;
    r[1][0] = -m[1][0] * inv;
    r[1][1] =  m[0][0] * inv;
    return r;
}

// ---------------------------------------------------------------------------
// Mat3 helpers (3×3) — 2D affine / homography
// ---------------------------------------------------------------------------

using Mat3f = Mat<3, 3, float>;
using Mat3d = Mat<3, 3, double>;

/// Determinant of a 3×3 matrix (cofactor expansion along first row).
template <Scalar T>
[[nodiscard]] constexpr T mat3_det(const Mat<3, 3, T>& m) noexcept {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

/// Inverse of a 3×3 matrix (closed form via adjugate). Returns zero if singular.
template <Scalar T>
[[nodiscard]] inline Mat<3, 3, T> mat3_inverse(const Mat<3, 3, T>& m) noexcept {
    T det = mat3_det(m);
    if (det == T(0)) return {};
    T inv = T(1) / det;
    Mat<3, 3, T> r{};
    // cofactors transposed (adjugate)
    r[0][0] = (m[1][1]*m[2][2] - m[1][2]*m[2][1]) * inv;
    r[0][1] = (m[0][2]*m[2][1] - m[0][1]*m[2][2]) * inv;
    r[0][2] = (m[0][1]*m[1][2] - m[0][2]*m[1][1]) * inv;
    r[1][0] = (m[1][2]*m[2][0] - m[1][0]*m[2][2]) * inv;
    r[1][1] = (m[0][0]*m[2][2] - m[0][2]*m[2][0]) * inv;
    r[1][2] = (m[0][2]*m[1][0] - m[0][0]*m[1][2]) * inv;
    r[2][0] = (m[1][0]*m[2][1] - m[1][1]*m[2][0]) * inv;
    r[2][1] = (m[0][1]*m[2][0] - m[0][0]*m[2][1]) * inv;
    r[2][2] = (m[0][0]*m[1][1] - m[0][1]*m[1][0]) * inv;
    return r;
}

/// Apply a 3×3 homography to a 2D point (uses homogeneous coordinates w=1).
/// Returns the 2D result after w-divide.
template <Scalar T>
[[nodiscard]] inline Vec<2, T>
mat3_apply_point(const Mat<3, 3, T>& m, const Vec<2, T>& p) noexcept {
    // Homogeneous input: [p.x, p.y, 1]
    T x = m[0][0]*p.data[0] + m[0][1]*p.data[1] + m[0][2];
    T y = m[1][0]*p.data[0] + m[1][1]*p.data[1] + m[1][2];
    T w = m[2][0]*p.data[0] + m[2][1]*p.data[1] + m[2][2];
    Vec<2, T> r{};
    if (w != T(0)) { r.data[0] = x / w; r.data[1] = y / w; }
    return r;
}

// ---------------------------------------------------------------------------
// Mat4 helpers (4×4)
// ---------------------------------------------------------------------------

using Mat4f = Mat<4, 4, float>;
using Mat4d = Mat<4, 4, double>;

/// Build a 4×4 translation matrix.
template <Scalar T>
[[nodiscard]] constexpr Mat<4, 4, T>
mat4_translation(T tx, T ty, T tz) noexcept {
    auto m = Mat<4, 4, T>::identity();
    m[0][3] = tx; m[1][3] = ty; m[2][3] = tz;
    return m;
}

/// Build a 4×4 scale matrix.
template <Scalar T>
[[nodiscard]] constexpr Mat<4, 4, T>
mat4_scale(T sx, T sy, T sz) noexcept {
    auto m = Mat<4, 4, T>::identity();
    m[0][0] = sx; m[1][1] = sy; m[2][2] = sz;
    return m;
}

/// Build a 4×4 rotation matrix around the X axis (radians).
template <Scalar T>
[[nodiscard]] inline Mat<4, 4, T> mat4_rotation_x(T angle) noexcept {
    auto m = Mat<4, 4, T>::identity();
    T c = std::cos(angle), s = std::sin(angle);
    m[1][1] =  c;  m[1][2] = -s;
    m[2][1] =  s;  m[2][2] =  c;
    return m;
}

/// Build a 4×4 rotation matrix around the Y axis (radians).
template <Scalar T>
[[nodiscard]] inline Mat<4, 4, T> mat4_rotation_y(T angle) noexcept {
    auto m = Mat<4, 4, T>::identity();
    T c = std::cos(angle), s = std::sin(angle);
    m[0][0] =  c;  m[0][2] =  s;
    m[2][0] = -s;  m[2][2] =  c;
    return m;
}

/// Build a 4×4 rotation matrix around the Z axis (radians).
template <Scalar T>
[[nodiscard]] inline Mat<4, 4, T> mat4_rotation_z(T angle) noexcept {
    auto m = Mat<4, 4, T>::identity();
    T c = std::cos(angle), s = std::sin(angle);
    m[0][0] =  c;  m[0][1] = -s;
    m[1][0] =  s;  m[1][1] =  c;
    return m;
}

/// Compose TRS: Translation * RotationZ * Scale (common 2D/3D shorthand).
/// For full 3D use mat4_translation * rotX * rotY * rotZ * mat4_scale separately.
template <Scalar T>
[[nodiscard]] inline Mat<4, 4, T>
mat4_trs(T tx, T ty, T tz,
         T rx, T ry, T rz,
         T sx, T sy, T sz) noexcept {
    auto t = mat4_translation<T>(tx, ty, tz);
    auto rx_m = mat4_rotation_x<T>(rx);
    auto ry_m = mat4_rotation_y<T>(ry);
    auto rz_m = mat4_rotation_z<T>(rz);
    auto s = mat4_scale<T>(sx, sy, sz);
    return t * rx_m * ry_m * rz_m * s;
}

/// Perspective projection matrix (right-handed, depth range [-1, +1]).
/// fov_y: vertical field of view in radians.
/// aspect: width / height.
/// near_z, far_z: clip plane distances (both > 0, near_z < far_z).
template <Scalar T>
[[nodiscard]] inline Mat<4, 4, T>
mat4_perspective(T fov_y, T aspect, T near_z, T far_z) noexcept {
    T f = T(1) / std::tan(fov_y / T(2));
    T range_inv = T(1) / (near_z - far_z);
    Mat<4, 4, T> m{};
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = (near_z + far_z) * range_inv;
    m[2][3] = T(2) * near_z * far_z * range_inv;
    m[3][2] = T(-1);
    return m;
}

/// Orthographic projection matrix (right-handed, depth range [-1, +1]).
template <Scalar T>
[[nodiscard]] constexpr Mat<4, 4, T>
mat4_ortho(T left, T right, T bottom, T top, T near_z, T far_z) noexcept {
    Mat<4, 4, T> m{};
    m[0][0] = T(2) / (right - left);
    m[1][1] = T(2) / (top - bottom);
    m[2][2] = T(-2) / (far_z - near_z);
    m[0][3] = -(right + left) / (right - left);
    m[1][3] = -(top + bottom) / (top - bottom);
    m[2][3] = -(far_z + near_z) / (far_z - near_z);
    m[3][3] = T(1);
    return m;
}

/// Inverse of a 4×4 matrix (closed form via cofactors). Returns zero if singular.
template <Scalar T>
[[nodiscard]] inline Mat<4, 4, T> mat4_inverse(const Mat<4, 4, T>& m) noexcept {
    // 2×2 sub-determinants
    T s0 = m[0][0]*m[1][1] - m[1][0]*m[0][1];
    T s1 = m[0][0]*m[1][2] - m[1][0]*m[0][2];
    T s2 = m[0][0]*m[1][3] - m[1][0]*m[0][3];
    T s3 = m[0][1]*m[1][2] - m[1][1]*m[0][2];
    T s4 = m[0][1]*m[1][3] - m[1][1]*m[0][3];
    T s5 = m[0][2]*m[1][3] - m[1][2]*m[0][3];

    T c5 = m[2][2]*m[3][3] - m[3][2]*m[2][3];
    T c4 = m[2][1]*m[3][3] - m[3][1]*m[2][3];
    T c3 = m[2][1]*m[3][2] - m[3][1]*m[2][2];
    T c2 = m[2][0]*m[3][3] - m[3][0]*m[2][3];
    T c1 = m[2][0]*m[3][2] - m[3][0]*m[2][2];
    T c0 = m[2][0]*m[3][1] - m[3][0]*m[2][1];

    T det = s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0;
    if (det == T(0)) return {};
    T inv = T(1) / det;

    Mat<4, 4, T> r{};
    r[0][0] = ( m[1][1]*c5 - m[1][2]*c4 + m[1][3]*c3) * inv;
    r[0][1] = (-m[0][1]*c5 + m[0][2]*c4 - m[0][3]*c3) * inv;
    r[0][2] = ( m[3][1]*s5 - m[3][2]*s4 + m[3][3]*s3) * inv;
    r[0][3] = (-m[2][1]*s5 + m[2][2]*s4 - m[2][3]*s3) * inv;

    r[1][0] = (-m[1][0]*c5 + m[1][2]*c2 - m[1][3]*c1) * inv;
    r[1][1] = ( m[0][0]*c5 - m[0][2]*c2 + m[0][3]*c1) * inv;
    r[1][2] = (-m[3][0]*s5 + m[3][2]*s2 - m[3][3]*s1) * inv;
    r[1][3] = ( m[2][0]*s5 - m[2][2]*s2 + m[2][3]*s1) * inv;

    r[2][0] = ( m[1][0]*c4 - m[1][1]*c2 + m[1][3]*c0) * inv;
    r[2][1] = (-m[0][0]*c4 + m[0][1]*c2 - m[0][3]*c0) * inv;
    r[2][2] = ( m[3][0]*s4 - m[3][1]*s2 + m[3][3]*s0) * inv;
    r[2][3] = (-m[2][0]*s4 + m[2][1]*s2 - m[2][3]*s0) * inv;

    r[3][0] = (-m[1][0]*c3 + m[1][1]*c1 - m[1][2]*c0) * inv;
    r[3][1] = ( m[0][0]*c3 - m[0][1]*c1 + m[0][2]*c0) * inv;
    r[3][2] = (-m[3][0]*s3 + m[3][1]*s1 - m[3][2]*s0) * inv;
    r[3][3] = ( m[2][0]*s3 - m[2][1]*s1 + m[2][2]*s0) * inv;
    return r;
}

}  // namespace ergo::math
