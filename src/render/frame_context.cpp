#include "ergo/render/frame_context.h"

#include <cmath>

namespace ergo::render {

namespace {

/// 円周率 (M_PI に依存せず自前で持つ)。
constexpr float kPi = 3.14159265358979323846f;

Vec3 normalize(const Vec3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.0f) return Vec3{0.0f, 0.0f, 0.0f};
    const float inv = 1.0f / len;
    return Vec3{v.x * inv, v.y * inv, v.z * inv};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
}

} // namespace

float deg_to_rad(float deg) {
    return deg * (kPi / 180.0f);
}

void mat4_identity(float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}

void look_at_rh(const Vec3& eye, const Vec3& center, const Vec3& up,
                float out[16]) {
    // KuzuSurvivors の follow_camera.cpp::look_at_rh をそのまま移植。
    // 右手系の forward = (center - eye) 正規化。
    const Vec3 f = normalize(Vec3{center.x - eye.x,
                                  center.y - eye.y,
                                  center.z - eye.z});
    Vec3 u = normalize(up);
    const Vec3 s = normalize(cross(f, u));
    u = cross(s, f);

    // column-major view matrix。
    out[ 0] = s.x;  out[ 4] = s.y;  out[ 8] = s.z;
    out[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    out[ 1] = u.x;  out[ 5] = u.y;  out[ 9] = u.z;
    out[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    out[ 2] = -f.x; out[ 6] = -f.y; out[10] = -f.z;
    out[14] =  (f.x * eye.x + f.y * eye.y + f.z * eye.z);
    out[ 3] = 0.0f; out[ 7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

void perspective_vk(float fov_deg, float aspect, float z_near, float z_far,
                    float out[16]) {
    // KuzuSurvivors の follow_camera.cpp::perspective_vk をそのまま移植。
    // Vulkan NDC: Y down, depth [0,1]。
    const float f = 1.0f / std::tan(deg_to_rad(fov_deg * 0.5f));
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0]  = f / aspect;
    out[5]  = -f;                              // Y flip (Vulkan)
    out[10] = z_far / (z_near - z_far);
    out[11] = -1.0f;
    out[14] = (z_near * z_far) / (z_near - z_far);
}

} // namespace ergo::render
