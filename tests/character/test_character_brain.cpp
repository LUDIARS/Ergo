#include "ergo/character/transform_brain.h"
#include "ergo/character/controls/manual_control.h"

#include "gtest/gtest.h"

#include <string>
#include <vector>

using namespace ergo::character;

namespace {

/// テスト用の素朴な IMoveSource (優先度 + 固定速度)。
class StubMove : public IMoveSource {
public:
    StubMove(int priority, Vec3 velocity, std::vector<std::string>* log = nullptr,
             std::string name = {})
        : priority_(priority), velocity_(velocity), log_(log), name_(std::move(name)) {}

    void set_priority(int p) { priority_ = p; }

    int  move_priority() const override { return priority_; }
    Vec3 move_velocity() const override { return velocity_; }

    void on_move_acquire_highest() override { push("acquire"); }
    void on_move_lose_highest() override { push("lose"); }
    void on_move_update_highest(float) override { push("update"); }

private:
    void push(const char* ev) {
        if (log_) log_->push_back(name_ + ":" + ev);
    }
    int   priority_;
    Vec3  velocity_;
    std::vector<std::string>* log_;
    std::string name_;
};

/// テスト用の固定 Effect。
class StubEffect : public IEffectSource {
public:
    explicit StubEffect(Vec3 v) : velocity_(v) {}
    Vec3 effect_velocity() const override { return velocity_; }
    void reset_effect_velocity() override { velocity_ = {}; }

private:
    Vec3 velocity_;
};

/// テスト用の順序記録 Check。
class StubUpdate : public IUpdateComponent {
public:
    StubUpdate(int order, std::vector<int>* log) : order_(order), log_(log) {}
    int  update_order() const override { return order_; }
    void on_update(float) override { log_->push_back(order_); }

private:
    int order_;
    std::vector<int>* log_;
};

} // namespace

TEST(CharacterBrain, HighestPriorityMoveWins) {
    Pose pose;
    TransformBrain brain(&pose);
    StubMove low(1, {1, 0, 0});
    StubMove high(5, {0, 0, 2});
    brain.add_move(&low);
    brain.add_move(&high);

    brain.update(1.0f);

    EXPECT_FLOAT_EQ(pose.position.x, 0.0f);
    EXPECT_FLOAT_EQ(pose.position.z, 2.0f);
    EXPECT_FLOAT_EQ(brain.current_speed(), 2.0f);
}

TEST(CharacterBrain, ZeroOrNegativePriorityIsAbsent) {
    Pose pose;
    TransformBrain brain(&pose);
    StubMove zero(0, {1, 0, 0});
    StubMove negative(-3, {0, 0, 1});
    brain.add_move(&zero);
    brain.add_move(&negative);

    brain.update(1.0f);

    EXPECT_FLOAT_EQ(pose.position.x, 0.0f);
    EXPECT_FLOAT_EQ(pose.position.z, 0.0f);
    EXPECT_FLOAT_EQ(brain.current_speed(), 0.0f);
}

TEST(CharacterBrain, PriorityLifecycleFires) {
    Pose pose;
    TransformBrain brain(&pose);
    std::vector<std::string> log;
    StubMove a(1, {1, 0, 0}, &log, "a");
    StubMove b(0, {0, 0, 1}, &log, "b");
    brain.add_move(&a);
    brain.add_move(&b);

    brain.update(0.016f);  // a が獲得
    b.set_priority(10);
    brain.update(0.016f);  // b に切替

    ASSERT_EQ(log.size(), 5u);
    EXPECT_EQ(log[0], "a:acquire");
    EXPECT_EQ(log[1], "a:update");
    EXPECT_EQ(log[2], "a:lose");
    EXPECT_EQ(log[3], "b:acquire");
    EXPECT_EQ(log[4], "b:update");
}

TEST(CharacterBrain, EffectsAreAdditive) {
    Pose pose;
    TransformBrain brain(&pose);
    StubMove move(1, {1, 0, 0});
    StubEffect gravity({0, -2, 0});
    StubEffect wind({0, 0, 3});
    brain.add_move(&move);
    brain.add_effect(&gravity);
    brain.add_effect(&wind);

    brain.update(1.0f);

    EXPECT_FLOAT_EQ(brain.control_velocity().x, 1.0f);
    EXPECT_FLOAT_EQ(brain.effect_velocity().y, -2.0f);
    EXPECT_FLOAT_EQ(brain.effect_velocity().z, 3.0f);
    EXPECT_FLOAT_EQ(pose.position.x, 1.0f);
    EXPECT_FLOAT_EQ(pose.position.y, -2.0f);
    EXPECT_FLOAT_EQ(pose.position.z, 3.0f);
}

TEST(CharacterBrain, WarpOverridesMoveAndResetsEffects) {
    Pose pose;
    TransformBrain brain(&pose);
    StubMove move(1, {1, 0, 0});
    StubEffect knockback({0, 0, 5});
    brain.add_move(&move);
    brain.add_effect(&knockback);

    brain.warp({100, 0, 100});
    brain.update(1.0f);

    EXPECT_FLOAT_EQ(pose.position.x, 100.0f);
    EXPECT_FLOAT_EQ(pose.position.z, 100.0f);

    // 位置 warp で Effect はリセット済み → 次フレームは Control のみ動く。
    brain.update(1.0f);
    EXPECT_FLOAT_EQ(pose.position.x, 101.0f);
    EXPECT_FLOAT_EQ(pose.position.z, 100.0f);
}

TEST(CharacterBrain, TurnInterpolatesShortestArc) {
    Pose pose;
    pose.yaw_deg = 350.0f;
    TransformBrain brain(&pose);
    ManualControl turn;
    turn.set_turn_priority(1);
    turn.set_target_yaw_deg(10.0f);
    turn.set_turn_speed_deg(100.0f);  // 100 deg/s
    brain.add_turn(&turn);

    brain.update(0.1f);  // 最大 10 度 — 350 → 360(=0) 方向へ進む

    // -10 度 (=350) から +10 度差 → 0 度 (wrap 後) に到達
    EXPECT_NEAR(pose.yaw_deg, 0.0f, 1e-3f);

    brain.update(0.1f);  // 残り 10 度進んで目標到達
    EXPECT_NEAR(pose.yaw_deg, 10.0f, 1e-3f);
}

TEST(CharacterBrain, NegativeTurnSpeedSnapsImmediately) {
    Pose pose;
    TransformBrain brain(&pose);
    ManualControl turn;
    turn.set_turn_priority(1);
    turn.set_target_yaw_deg(135.0f);
    turn.set_turn_speed_deg(-1.0f);
    brain.add_turn(&turn);

    brain.update(0.001f);

    EXPECT_NEAR(pose.yaw_deg, 135.0f, 1e-3f);
}

TEST(CharacterBrain, NoTurnSourceKeepsYaw) {
    Pose pose;
    pose.yaw_deg = 42.0f;
    TransformBrain brain(&pose);

    brain.update(1.0f);

    EXPECT_FLOAT_EQ(pose.yaw_deg, 42.0f);
}

TEST(CharacterBrain, WarpYawOverridesTurn) {
    Pose pose;
    TransformBrain brain(&pose);
    ManualControl turn;
    turn.set_turn_priority(1);
    turn.set_target_yaw_deg(90.0f);
    brain.add_turn(&turn);

    brain.warp_yaw(-90.0f);
    brain.update(0.016f);

    EXPECT_NEAR(pose.yaw_deg, -90.0f, 1e-3f);
}

TEST(CharacterBrain, UpdateComponentsRunInOrder) {
    Pose pose;
    TransformBrain brain(&pose);
    std::vector<int> log;
    StubUpdate c30(30, &log);
    StubUpdate c10(10, &log);
    StubUpdate c20(20, &log);
    brain.add_update(&c30);
    brain.add_update(&c10);
    brain.add_update(&c20);

    brain.update(0.016f);

    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], 10);
    EXPECT_EQ(log[1], 20);
    EXPECT_EQ(log[2], 30);
}

TEST(CharacterBrain, MoveFilterShapesDelta) {
    Pose pose;
    TransformBrain brain(&pose);
    StubMove move(1, {3, 0, 4});
    brain.add_move(&move);
    // 壁ずり的に X 成分だけ通すフィルタ。
    brain.set_move_filter([](const Vec3&, const Vec3& delta) {
        return Vec3{delta.x, 0.0f, 0.0f};
    });

    brain.update(1.0f);

    EXPECT_FLOAT_EQ(pose.position.x, 3.0f);
    EXPECT_FLOAT_EQ(pose.position.z, 0.0f);
}

TEST(CharacterBrain, RemovedSourceIsNotConsulted) {
    Pose pose;
    TransformBrain brain(&pose);
    StubMove move(1, {1, 0, 0});
    brain.add_move(&move);
    brain.update(1.0f);
    EXPECT_FLOAT_EQ(pose.position.x, 1.0f);

    brain.remove_move(&move);
    brain.update(1.0f);
    EXPECT_FLOAT_EQ(pose.position.x, 1.0f);
}
