#pragma once
/// lesson_action_feel — a 2D float vector for the arena game.
///
/// The sample is built as C++17 (like the repository); ergo_math's Vec needs
/// C++20 concepts, so this example keeps the few operations it needs here.
#include <cmath>

namespace lesson_action_feel {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    Vec2&          operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2&          operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    Vec2&          operator*=(float s) { x *= s; y *= s; return *this; }
    constexpr bool operator==(Vec2 o) const { return x == o.x && y == o.y; }

    constexpr float dot(Vec2 o) const { return x * o.x + y * o.y; }
    float           length() const { return std::sqrt(x * x + y * y); }
    constexpr bool  is_zero() const { return x == 0.0f && y == 0.0f; }
    /// Unit vector, or zero when the length is (near) zero.
    Vec2 normalized() const {
        const float len = length();
        if (len < 1e-6f) return {};
        return {x / len, y / len};
    }
};

} // namespace lesson_action_feel
