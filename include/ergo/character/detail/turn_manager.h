#pragma once

/// ergo::character::detail — ITurnSource の優先度選択 + yaw 補間。
///
/// TCC の `TurnManager` に対応する。BrainBase の内部実装。
///
/// Spec: spec/module/character.md

#include "ergo/character/turn_source.h"

#include <vector>

namespace ergo::character::detail {

class TurnManager {
public:
    void add(ITurnSource* source);
    void remove(ITurnSource* source);

    /// 最高優先度 (>0) の source を再評価し、lifecycle フックを発火する。
    void update_highest(float dt);

    /// current_yaw_deg から採用 source の目標 yaw へ最短弧で補間した
    /// 「このフレームで適用すべき yaw」を計算する。turn_speed_deg() が
    /// 負なら即時スナップ。採用 source が無ければ何もしない。
    void calculate_angle(float dt, float current_yaw_deg);

    bool  has_highest() const { return current_ != nullptr; }
    float target_yaw_deg() const { return target_yaw_deg_; }
    float next_yaw_deg() const { return next_yaw_deg_; }
    /// 現在向きと目標の最短弧差 (deg)。
    float delta_turn_deg() const { return delta_turn_deg_; }
    ITurnSource* current() const { return current_; }

private:
    std::vector<ITurnSource*> sources_;
    ITurnSource* current_ = nullptr;
    float target_yaw_deg_ = 0.0f;
    float next_yaw_deg_   = 0.0f;
    float delta_turn_deg_ = 0.0f;
};

} // namespace ergo::character::detail
