#include "gtest/gtest.h"

#include "ergo/render/frame_context.h"

#include <cmath>

using namespace ergo::render;

namespace {

constexpr float kEps = 1e-4f;

} // namespace

TEST(FrameContext, DefaultsAreSane) {
    FrameContext fc;
    EXPECT_FLOAT_EQ(fc.dt, 0.0f);
    EXPECT_EQ(fc.extent.width,  0u);
    EXPECT_EQ(fc.extent.height, 0u);
    EXPECT_EQ(fc.frame_index, 0u);
    // view / proj は単位行列で初期化される。
    EXPECT_FLOAT_EQ(fc.view[0],  1.0f);
    EXPECT_FLOAT_EQ(fc.view[5],  1.0f);
    EXPECT_FLOAT_EQ(fc.view[10], 1.0f);
    EXPECT_FLOAT_EQ(fc.view[15], 1.0f);
    EXPECT_FLOAT_EQ(fc.view[1],  0.0f);
}

TEST(CameraMath, DegToRad) {
    EXPECT_NEAR(deg_to_rad(180.0f), 3.14159265f, kEps);
    EXPECT_NEAR(deg_to_rad(90.0f),  1.57079632f, kEps);
    EXPECT_NEAR(deg_to_rad(0.0f),   0.0f,        kEps);
}

TEST(CameraMath, Mat4Identity) {
    float m[16];
    for (int i = 0; i < 16; ++i) m[i] = 7.0f;
    mat4_identity(m);
    for (int i = 0; i < 16; ++i) {
        const float expect = (i % 5 == 0) ? 1.0f : 0.0f;
        EXPECT_FLOAT_EQ(m[i], expect);
    }
}

TEST(CameraMath, LookAtFromOriginDownNegZ) {
    // 原点から -Z を向くカメラ。 forward = (0,0,-1)。
    float m[16];
    look_at_rh(Vec3{0, 0, 0}, Vec3{0, 0, -1}, Vec3{0, 1, 0}, m);
    // eye が原点なので translation 列は 0。
    EXPECT_NEAR(m[12], 0.0f, kEps);
    EXPECT_NEAR(m[13], 0.0f, kEps);
    EXPECT_NEAR(m[14], 0.0f, kEps);
    // -Z forward を見るとき view 行列は実質単位行列に近い。
    EXPECT_NEAR(m[0],  1.0f, kEps);   // right.x
    EXPECT_NEAR(m[5],  1.0f, kEps);   // up.y
    EXPECT_NEAR(m[10], 1.0f, kEps);   // -forward.z = -(-1)
}

TEST(CameraMath, LookAtTranslationEncodesEye) {
    // eye を +Z に動かすと translation 成分が現れる。
    float m[16];
    look_at_rh(Vec3{0, 0, 5}, Vec3{0, 0, 0}, Vec3{0, 1, 0}, m);
    // forward = (0,0,-1), so -forward = (0,0,1); m[14] = f.z*eye.z = -1*5 = -5.
    EXPECT_NEAR(m[14], -5.0f, kEps);
}

TEST(CameraMath, PerspectiveVkYFlipAndDepthRange) {
    float m[16];
    perspective_vk(60.0f, 16.0f / 9.0f, 0.1f, 100.0f, m);
    // Vulkan: Y は反転されるので m[5] は負。
    EXPECT_LT(m[5], 0.0f);
    // m[11] は -1 (透視除算用)。
    EXPECT_FLOAT_EQ(m[11], -1.0f);
    // m[0] は正 (X スケール)。
    EXPECT_GT(m[0], 0.0f);
    // 深度マッピング: m[10] = zf/(zn-zf) は負、 m[14] = zn*zf/(zn-zf) も負。
    EXPECT_LT(m[10], 0.0f);
    EXPECT_LT(m[14], 0.0f);
}

TEST(CameraMath, PerspectiveAspectScalesX) {
    float wide[16], narrow[16];
    perspective_vk(60.0f, 2.0f, 0.1f, 100.0f, wide);
    perspective_vk(60.0f, 1.0f, 0.1f, 100.0f, narrow);
    // aspect が大きいほど m[0] (= f/aspect) は小さくなる。
    EXPECT_LT(wide[0], narrow[0]);
}
