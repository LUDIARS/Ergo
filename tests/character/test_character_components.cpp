#include "ergo/character/checks/manual_ground_check.h"
#include "ergo/character/controls/move_control.h"
#include "ergo/character/effects/extra_force.h"
#include "ergo/character/effects/gravity.h"
#include "ergo/character/transform_brain.h"

#include "gtest/gtest.h"

using namespace ergo::character;

TEST(MoveControl, VelocityFromInputAndSpeed) {
    MoveControl mc;
    mc.set_speed(4.0f);
    mc.set_move_input({1, 0, 0});
    EXPECT_FLOAT_EQ(mc.move_velocity().x, 4.0f);
    EXPECT_TRUE(mc.is_moving());
}

TEST(MoveControl, BelowThresholdStops) {
    MoveControl mc;
    mc.set_move_input({0.05f, 0, 0});  // 既定閾値 0.1 未満
    EXPECT_FALSE(mc.is_moving());
    EXPECT_FLOAT_EQ(mc.move_velocity().x, 0.0f);
}

TEST(MoveControl, TargetYawFollowsInputDirection) {
    MoveControl mc;
    mc.set_move_input({1, 0, 0});  // +X = 90 度
    EXPECT_NEAR(mc.target_yaw_deg(), 90.0f, 1e-3f);
    mc.set_move_input({0, 0, -1});  // -Z = 180 度
    EXPECT_NEAR(std::fabs(mc.target_yaw_deg()), 180.0f, 1e-3f);
}

TEST(MoveControl, KeepsLastYawWhenInputStops) {
    MoveControl mc;
    mc.set_move_input({1, 0, 0});
    mc.set_move_input({0, 0, 0});
    EXPECT_NEAR(mc.target_yaw_deg(), 90.0f, 1e-3f);
    EXPECT_GT(mc.turn_priority(), 0);  // 一度向いたら Turn は存在し続ける
}

TEST(MoveControl, TurnAbsentUntilFirstInput) {
    MoveControl mc;
    EXPECT_EQ(mc.turn_priority(), 0);  // 初期向きを壊さない
}

TEST(Gravity, AcceleratesWhileAirborne) {
    Gravity g;
    g.set_gravity(10.0f);
    ManualGroundCheck ground;
    ground.set_on_ground(false);
    g.set_ground_contact(&ground);

    g.on_update(1.0f);
    EXPECT_FLOAT_EQ(g.fall_speed(), -10.0f);
    g.on_update(1.0f);
    EXPECT_FLOAT_EQ(g.fall_speed(), -20.0f);
    EXPECT_FALSE(g.is_grounded());
}

TEST(Gravity, LandingStopsFallAndFiresCallback) {
    Gravity g;
    g.set_gravity(10.0f);
    ManualGroundCheck ground;
    ground.set_on_ground(false);
    g.set_ground_contact(&ground);

    float landed_speed = 0.0f;
    g.on_land([&](float v) { landed_speed = v; });

    g.on_update(1.0f);  // -10
    ground.set_on_ground(true);
    g.on_update(0.016f);  // 着地

    EXPECT_TRUE(g.is_grounded());
    EXPECT_FLOAT_EQ(g.fall_speed(), 0.0f);
    EXPECT_FLOAT_EQ(landed_speed, -10.0f);
}

TEST(Gravity, UpwardVelocityLeavesGround) {
    Gravity g;
    ManualGroundCheck ground;
    ground.set_on_ground(true);
    g.set_ground_contact(&ground);

    bool left = false;
    g.on_leave([&] { left = true; });

    g.on_update(0.016f);  // 接地状態へ
    g.add_upward_velocity(5.0f);

    EXPECT_TRUE(left);
    EXPECT_FALSE(g.is_grounded());
    EXPECT_FLOAT_EQ(g.fall_speed(), 5.0f);
}

TEST(Gravity, NoGroundContactMeansAlwaysAirborne) {
    Gravity g;
    g.set_gravity(10.0f);
    g.on_update(1.0f);
    EXPECT_FLOAT_EQ(g.fall_speed(), -10.0f);
}

TEST(ExtraForce, ImpulseDecays) {
    ExtraForce f;
    f.set_damping(1.0f);
    f.add_impulse({10, 0, 0});
    f.on_update(0.5f);  // ×0.5
    EXPECT_FLOAT_EQ(f.velocity().x, 5.0f);
    f.on_update(2.0f);  // k は 0 でクランプ
    EXPECT_FLOAT_EQ(f.velocity().x, 0.0f);
}

/// 統合: MoveControl + Gravity + ExtraForce を 1 つの Brain に載せて
/// 「歩きながらノックバックを受けて落下する」合成が成立すること。
TEST(CharacterIntegration, WalkKnockbackGravityCompose) {
    Pose pose;
    TransformBrain brain(&pose);

    MoveControl mc;
    mc.set_speed(2.0f);
    Gravity g;
    g.set_gravity(10.0f);
    ManualGroundCheck ground;
    ground.set_on_ground(false);
    g.set_ground_contact(&ground);
    ExtraForce force;
    force.set_damping(0.0f);  // 減衰なしで検証しやすく

    brain.add_move(&mc);
    brain.add_turn(&mc);
    brain.add_effect(&g);
    brain.add_effect(&force);
    brain.add_update(&g);
    brain.add_update(&force);

    mc.set_move_input({1, 0, 0});
    force.add_impulse({0, 0, 3});

    brain.update(1.0f);

    EXPECT_FLOAT_EQ(pose.position.x, 2.0f);    // 歩行
    EXPECT_FLOAT_EQ(pose.position.z, 3.0f);    // ノックバック (加算)
    EXPECT_FLOAT_EQ(pose.position.y, -10.0f);  // 重力 (加算)
    // 向きも入力方向 (+X = 90 度) へ向かっている (既定 720 deg/s × 1s で到達)。
    EXPECT_NEAR(pose.yaw_deg, 90.0f, 1e-3f);
}
