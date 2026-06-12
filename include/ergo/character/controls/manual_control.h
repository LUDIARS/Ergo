#pragma once

/// ergo::character — ManualControl: ホストが速度・向きを直接指定する Control。
///
/// TCC の `ManualControl` / `ManualTurn` に対応する。カットシーン・AI・
/// スクリプト移動など「入力ではなく外部ロジックが希望値を握る」場面で使う。
/// priority を 0 にすれば不在扱いに戻る。
///
/// Spec: spec/module/character.md

#include "ergo/character/move_source.h"
#include "ergo/character/turn_source.h"

namespace ergo::character {

class ManualControl : public IMoveSource, public ITurnSource {
public:
    void set_velocity(const Vec3& v) { velocity_ = v; }
    void set_target_yaw_deg(float yaw) { target_yaw_deg_ = yaw; }
    void set_move_priority(int p) { move_priority_ = p; }
    void set_turn_priority(int p) { turn_priority_ = p; }
    void set_turn_speed_deg(float deg_per_second) { turn_speed_deg_ = deg_per_second; }

    // ── IMoveSource ──────────────────────────────────────────────────
    int  move_priority() const override { return move_priority_; }
    Vec3 move_velocity() const override { return velocity_; }

    // ── ITurnSource ──────────────────────────────────────────────────
    int   turn_priority() const override { return turn_priority_; }
    float turn_speed_deg() const override { return turn_speed_deg_; }
    float target_yaw_deg() const override { return target_yaw_deg_; }

private:
    Vec3  velocity_{};
    float target_yaw_deg_ = 0.0f;
    int   move_priority_  = 0;   // 既定は不在 (明示的に上げて使う)
    int   turn_priority_  = 0;
    float turn_speed_deg_ = -1.0f;  // 既定は即時スナップ
};

} // namespace ergo::character
