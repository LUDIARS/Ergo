#pragma once

/// ergo::character — MoveControl: 地上移動の標準 Control。
///
/// TCC の `MoveControl` に対応する。移動入力 (XZ スティック値) を速度に
/// 変換し (`IMoveSource`)、入力方向へキャラを向ける (`ITurnSource`)。
/// 入力が無い間は速度 0 のまま最後の向きを保持する (TCC と同じ挙動)。
///
/// Spec: spec/module/character.md

#include "ergo/character/move_source.h"
#include "ergo/character/turn_source.h"

namespace ergo::character {

class MoveControl : public IMoveSource, public ITurnSource {
public:
    /// 移動入力 (XZ 平面、長さ 0..1 のスティック値)。毎フレーム供給する。
    void set_move_input(const Vec3& input_xz);

    void set_speed(float units_per_second) { speed_ = units_per_second; }
    float speed() const { return speed_; }

    void set_move_priority(int p) { move_priority_ = p; }
    void set_turn_priority(int p) { turn_priority_ = p; }
    void set_turn_speed_deg(float deg_per_second) { turn_speed_deg_ = deg_per_second; }

    /// 現在の入力が「移動中」とみなされる長さ閾値 (既定 0.1)。
    void set_input_threshold(float t) { input_threshold_ = t; }

    bool is_moving() const { return moving_; }

    // ── IMoveSource ──────────────────────────────────────────────────
    int  move_priority() const override { return move_priority_; }
    Vec3 move_velocity() const override;

    // ── ITurnSource ──────────────────────────────────────────────────
    /// 一度も入力が無いうちは 0 (= 不在) を返し、他の Turn に向きを譲る。
    int   turn_priority() const override;
    float turn_speed_deg() const override { return turn_speed_deg_; }
    float target_yaw_deg() const override { return target_yaw_deg_; }

private:
    Vec3  input_{};
    float speed_           = 4.0f;
    int   move_priority_   = 1;
    int   turn_priority_   = 1;
    float turn_speed_deg_  = 720.0f;
    float input_threshold_ = 0.1f;
    float target_yaw_deg_  = 0.0f;
    bool  moving_          = false;
    bool  has_faced_       = false;  // 一度でも入力方向を向いたか
};

} // namespace ergo::character
