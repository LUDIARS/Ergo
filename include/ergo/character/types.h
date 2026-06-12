#pragma once

/// ergo::character — 基礎型 (Vec3 / Pose) と角度ユーティリティ。
///
/// ergo_character はエンジン非依存の最下層モジュールなので、数学型も
/// ここで自己完結させる (他モジュール / ホストの math 型へは依存しない)。
///
/// Spec: spec/module/character.md

#include <cmath>

namespace ergo::character {

/// 最小限の 3 成分ベクトル。キャラ制御の速度・位置にのみ使う。
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float length_xz() const { return std::sqrt(x * x + z * z); }
};

/// キャラクターの最終 transform。ホストが所有し、TransformBrain が書き込む。
/// 回転は TCC と同じく Y 軸 yaw の 1 自由度のみ (deg、Z+ 方向が 0 度)。
struct Pose {
    Vec3  position{};
    float yaw_deg = 0.0f;
};

/// 角度 (deg) を [-180, 180) に正規化する。角度差の最短弧判定に使う。
inline float wrap_angle_deg(float deg) {
    deg = std::fmod(deg + 180.0f, 360.0f);
    if (deg < 0.0f) deg += 360.0f;
    return deg - 180.0f;
}

/// current から target へ最短弧方向に最大 max_step_deg だけ進めた角度を返す。
/// max_step_deg が差分以上なら target に到達する。
inline float move_toward_angle_deg(float current, float target, float max_step_deg) {
    const float delta = wrap_angle_deg(target - current);
    if (std::fabs(delta) <= max_step_deg) return target;
    return current + (delta > 0.0f ? max_step_deg : -max_step_deg);
}

/// XZ 平面の方向ベクトル → yaw (deg、Z+ が 0 度・X+ が 90 度)。
inline float direction_to_yaw_deg(const Vec3& dir) {
    return std::atan2(dir.x, dir.z) * (180.0f / 3.14159265358979323846f);
}

} // namespace ergo::character
