#include "gtest/gtest.h"

#include "ergo/stickfig/stick_figure.h"

#include <cmath>

using namespace ergo::stickfig;

namespace {

constexpr float kEps = 1e-4f;

// メッシュの全インデックスが頂点配列の範囲内かを検証する。
bool indices_in_range(const Mesh& m) {
    for (uint32_t idx : m.idxs) {
        if (idx >= m.verts.size()) return false;
    }
    return true;
}

// 全法線が概ね単位ベクトルかを検証する。
bool normals_unit(const Mesh& m) {
    for (const auto& v : m.verts) {
        const float len = std::sqrt(v.normal[0] * v.normal[0] +
                                    v.normal[1] * v.normal[1] +
                                    v.normal[2] * v.normal[2]);
        if (std::fabs(len - 1.0f) > 1e-2f) return false;
    }
    return true;
}

} // namespace

TEST(StickFigureSphere, ProducesValidMesh) {
    const Mesh m = generate_sphere(0.5f, 16, 12);
    EXPECT_GT(m.verts.size(), 0u);
    EXPECT_GT(m.idxs.size(), 0u);
    EXPECT_EQ(m.idxs.size() % 3u, 0u);
    EXPECT_TRUE(indices_in_range(m));
    EXPECT_TRUE(normals_unit(m));
}

TEST(StickFigureSphere, ClampsDegenerateParams) {
    // segments / rings が小さすぎても落ちず最小値にクランプされる。
    const Mesh m = generate_sphere(1.0f, 1, 0);
    EXPECT_GT(m.verts.size(), 0u);
    EXPECT_TRUE(indices_in_range(m));
}

TEST(StickFigureCapsule, TotalHeightMatchesLengthPlusCaps) {
    const float radius = 0.1f;
    const float length = 1.0f;
    const Mesh m = generate_capsule(radius, length, 12, 4);
    EXPECT_GT(m.verts.size(), 0u);
    EXPECT_EQ(m.idxs.size() % 3u, 0u);
    EXPECT_TRUE(indices_in_range(m));

    float min_y = 1e9f, max_y = -1e9f;
    for (const auto& v : m.verts) {
        min_y = std::fmin(min_y, v.pos[1]);
        max_y = std::fmax(max_y, v.pos[1]);
    }
    // 全長 = 円柱長 + 2 * 半径。
    EXPECT_NEAR(max_y - min_y, length + 2.0f * radius, kEps);
    // 最大半径は radius を超えない。
    for (const auto& v : m.verts) {
        const float r = std::sqrt(v.pos[0] * v.pos[0] + v.pos[2] * v.pos[2]);
        EXPECT_LE(r, radius + kEps);
    }
}

TEST(StickFigureCapsule, ZeroLengthIsSphereLike) {
    const Mesh m = generate_capsule(0.2f, 0.0f, 10, 3);
    EXPECT_GT(m.verts.size(), 0u);
    EXPECT_TRUE(indices_in_range(m));
}

TEST(StickFigure, BuildsSixParts) {
    StickFigureParams p;
    const auto parts = generate_stick_figure(p);
    ASSERT_EQ(parts.size(), 6u);  // torso + 2 legs + 2 arms + head
    for (const auto& part : parts) {
        EXPECT_GT(part.mesh.verts.size(), 0u);
        EXPECT_GT(part.mesh.idxs.size(), 0u);
        EXPECT_TRUE(indices_in_range(part.mesh));
        // model 行列の末尾は 1 (affine)。
        EXPECT_FLOAT_EQ(part.model[15], 1.0f);
        // alpha は不透明。
        EXPECT_FLOAT_EQ(part.color[3], 1.0f);
    }
}

TEST(StickFigure, RespectsHeightParam) {
    StickFigureParams tall;
    tall.height = 2.4f;
    const auto parts = generate_stick_figure(tall);
    // 頭部パーツの並進 y は全高に応じて高くなる。
    const auto& head = parts.back();
    EXPECT_STREQ(head.name, "head");
    EXPECT_GT(head.model[13], 2.0f);
}
