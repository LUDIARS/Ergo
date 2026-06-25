#include "ergo/stickfig/stick_figure.h"

#include <array>
#include <cmath>

namespace ergo::stickfig {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Vec3 { float x, y, z; };

Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(const Vec3& a, float s)     { return {a.x * s, a.y * s, a.z * s}; }
float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 normalize(const Vec3& a) {
    const float len = length(a);
    if (len < 1e-8f) return {0.0f, 1.0f, 0.0f};
    return scale(a, 1.0f / len);
}

/// 列優先 4x4 単位行列。
void identity(float m[16]) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/// 列優先 4x4: 並進のみ。
void translation(float m[16], const Vec3& t) {
    identity(m);
    m[12] = t.x; m[13] = t.y; m[14] = t.z;
}

/// +Y 軸方向のカプセル/メッシュを、 線分 A->B に沿うよう配置する列優先行列を
/// 書き込む。 回転 (ローカル +Y -> dir) + 中点への並進。
void place_between(float m[16], const Vec3& a, const Vec3& b) {
    const Vec3 dir    = normalize(sub(b, a));
    const Vec3 center = scale(add(a, b), 0.5f);

    // dir が Y 軸とほぼ平行なときは別の基準ベクトルを選んで退化を避ける。
    const Vec3 ref = (std::fabs(dir.y) > 0.99f) ? Vec3{0.0f, 0.0f, 1.0f}
                                                : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 x_axis = normalize(cross(ref, dir));
    const Vec3 z_axis = cross(x_axis, dir);

    // 列: col0 = X-axis, col1 = Y-axis(=dir), col2 = Z-axis, col3 = center。
    m[0] = x_axis.x; m[1] = x_axis.y; m[2] = x_axis.z; m[3] = 0.0f;
    m[4] = dir.x;    m[5] = dir.y;    m[6] = dir.z;    m[7] = 0.0f;
    m[8] = z_axis.x; m[9] = z_axis.y; m[10] = z_axis.z; m[11] = 0.0f;
    m[12] = center.x; m[13] = center.y; m[14] = center.z; m[15] = 1.0f;
}

/// 線分 A->B を全長とするカプセルパーツを作る (円柱長 = |AB| - 2r)。
StickPart make_bone(const char* name, const Vec3& a, const Vec3& b,
                    float radius, const std::array<float, 4>& color,
                    int segments) {
    const float dist = length(sub(b, a));
    const float cyl  = std::fmax(0.0f, dist - 2.0f * radius);

    StickPart part{};
    part.name = name;
    part.mesh = generate_capsule(radius, cyl, segments);
    for (int i = 0; i < 4; ++i) part.color[i] = color[i];
    place_between(part.model, a, b);
    return part;
}

} // namespace

Mesh generate_sphere(float radius, int segments, int rings) {
    if (segments < 3) segments = 3;
    if (rings < 2)    rings = 2;

    Mesh mesh;
    // 緯度 ring = 0..rings (極を含む)、 経度 seg = 0..segments (継ぎ目を複製)。
    for (int r = 0; r <= rings; ++r) {
        const float v     = static_cast<float>(r) / static_cast<float>(rings);
        const float phi   = v * kPi;            // 0..pi (北極->南極)
        const float y     = std::cos(phi);
        const float rxz   = std::sin(phi);
        for (int s = 0; s <= segments; ++s) {
            const float u     = static_cast<float>(s) / static_cast<float>(segments);
            const float theta = u * 2.0f * kPi;
            const float nx = rxz * std::cos(theta);
            const float ny = y;
            const float nz = rxz * std::sin(theta);
            Vertex vert{};
            vert.pos[0]    = nx * radius;
            vert.pos[1]    = ny * radius;
            vert.pos[2]    = nz * radius;
            vert.normal[0] = nx;
            vert.normal[1] = ny;
            vert.normal[2] = nz;
            mesh.verts.push_back(vert);
        }
    }

    const int stride = segments + 1;
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            const uint32_t i0 = static_cast<uint32_t>(r * stride + s);
            const uint32_t i1 = static_cast<uint32_t>(r * stride + s + 1);
            const uint32_t i2 = static_cast<uint32_t>((r + 1) * stride + s);
            const uint32_t i3 = static_cast<uint32_t>((r + 1) * stride + s + 1);
            mesh.idxs.push_back(i0); mesh.idxs.push_back(i2); mesh.idxs.push_back(i1);
            mesh.idxs.push_back(i1); mesh.idxs.push_back(i2); mesh.idxs.push_back(i3);
        }
    }
    return mesh;
}

Mesh generate_capsule(float radius, float length, int segments, int cap_rings) {
    if (segments < 3)  segments = 3;
    if (cap_rings < 1) cap_rings = 1;
    if (length < 0.0f) length = 0.0f;

    const float half = length * 0.5f;  // 円柱の上下端 (+/- half) の y
    Mesh mesh;
    const int stride = segments + 1;

    // 縦方向リング列を順に積む:
    //   下半球 (cap_rings) -> 円柱の上下 2 リング -> 上半球 (cap_rings)。
    // 各リングを (cos,sin) 円周上に segments+1 頂点で配置する。
    struct Ring { float y; float r; float ny; bool hemi; };
    std::vector<Ring> ring_defs;

    // 下半球: phi = pi/2 (赤道) -> pi (南極)。 法線は球面方向。
    for (int r = 0; r <= cap_rings; ++r) {
        const float t   = static_cast<float>(r) / static_cast<float>(cap_rings);
        const float phi = kPi * 0.5f + t * (kPi * 0.5f);   // pi/2..pi
        ring_defs.push_back({-half + std::cos(phi) * radius,
                             std::sin(phi) * radius,
                             std::cos(phi), true});
    }
    // 円柱: 側面法線は水平。 上下端のリングを 1 つずつ。
    ring_defs.push_back({-half, radius, 0.0f, false});
    ring_defs.push_back({ half, radius, 0.0f, false});
    // 上半球: phi = pi/2 (赤道) -> 0 (北極)。
    for (int r = 0; r <= cap_rings; ++r) {
        const float t   = static_cast<float>(r) / static_cast<float>(cap_rings);
        const float phi = kPi * 0.5f - t * (kPi * 0.5f);   // pi/2..0
        ring_defs.push_back({ half + std::cos(phi) * radius,
                             std::sin(phi) * radius,
                             std::cos(phi), true});
    }

    for (const Ring& ring : ring_defs) {
        for (int s = 0; s <= segments; ++s) {
            const float u     = static_cast<float>(s) / static_cast<float>(segments);
            const float theta = u * 2.0f * kPi;
            const float cx = std::cos(theta);
            const float cz = std::sin(theta);
            Vertex vert{};
            vert.pos[0] = cx * ring.r;
            vert.pos[1] = ring.y;
            vert.pos[2] = cz * ring.r;
            if (ring.hemi) {
                // 半球: 法線は半球中心 (+/- half) からの方向。
                const float nr = std::sqrt(std::fmax(0.0f, 1.0f - ring.ny * ring.ny));
                vert.normal[0] = cx * nr;
                vert.normal[1] = ring.ny;
                vert.normal[2] = cz * nr;
            } else {
                vert.normal[0] = cx;
                vert.normal[1] = 0.0f;
                vert.normal[2] = cz;
            }
            mesh.verts.push_back(vert);
        }
    }

    const int ring_count = static_cast<int>(ring_defs.size());
    for (int r = 0; r < ring_count - 1; ++r) {
        for (int s = 0; s < segments; ++s) {
            const uint32_t i0 = static_cast<uint32_t>(r * stride + s);
            const uint32_t i1 = static_cast<uint32_t>(r * stride + s + 1);
            const uint32_t i2 = static_cast<uint32_t>((r + 1) * stride + s);
            const uint32_t i3 = static_cast<uint32_t>((r + 1) * stride + s + 1);
            mesh.idxs.push_back(i0); mesh.idxs.push_back(i2); mesh.idxs.push_back(i1);
            mesh.idxs.push_back(i1); mesh.idxs.push_back(i2); mesh.idxs.push_back(i3);
        }
    }
    return mesh;
}

std::vector<StickPart> generate_stick_figure(const StickFigureParams& p) {
    const float H = p.height;

    const float foot_y        = 0.0f;
    const float hip_y         = H * 0.50f;
    const float shoulder_y    = H * 0.82f;
    const float hip_half      = p.limb_radius * 1.6f;
    const float shoulder_half = p.arm_span * 0.10f;
    const float hand_x        = p.arm_span * 0.5f;
    const float hand_y        = shoulder_y - H * 0.18f;
    const float head_cy       = shoulder_y + p.head_radius + H * 0.04f;
    const float torso_radius  = p.limb_radius * 1.5f;

    const std::array<float, 4> col_torso = {0.25f, 0.45f, 0.85f, 1.0f};
    const std::array<float, 4> col_leg   = {0.85f, 0.30f, 0.25f, 1.0f};
    const std::array<float, 4> col_arm   = {0.30f, 0.75f, 0.40f, 1.0f};

    std::vector<StickPart> parts;
    parts.reserve(6);

    // 胴
    parts.push_back(make_bone("torso",
        {0.0f, hip_y, 0.0f}, {0.0f, shoulder_y, 0.0f},
        torso_radius, col_torso, p.segments));

    // 両脚
    parts.push_back(make_bone("leg_l",
        {-hip_half, hip_y, 0.0f}, {-hip_half, foot_y, 0.0f},
        p.limb_radius, col_leg, p.segments));
    parts.push_back(make_bone("leg_r",
        { hip_half, hip_y, 0.0f}, { hip_half, foot_y, 0.0f},
        p.limb_radius, col_leg, p.segments));

    // 両腕
    parts.push_back(make_bone("arm_l",
        {-shoulder_half, shoulder_y, 0.0f}, {-hand_x, hand_y, 0.0f},
        p.limb_radius, col_arm, p.segments));
    parts.push_back(make_bone("arm_r",
        { shoulder_half, shoulder_y, 0.0f}, { hand_x, hand_y, 0.0f},
        p.limb_radius, col_arm, p.segments));

    // 頭
    StickPart head{};
    head.name = "head";
    head.mesh = generate_sphere(p.head_radius, p.segments + 4, p.segments);
    head.color[0] = 0.95f; head.color[1] = 0.80f; head.color[2] = 0.65f; head.color[3] = 1.0f;
    translation(head.model, {0.0f, head_cy, 0.0f});
    parts.push_back(std::move(head));

    return parts;
}

} // namespace ergo::stickfig
