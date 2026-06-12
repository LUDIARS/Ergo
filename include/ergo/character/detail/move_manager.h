#pragma once

/// ergo::character::detail — IMoveSource の優先度選択 + 速度確定。
///
/// TCC の `MoveManager` に対応する。BrainBase の内部実装で、ホストが
/// 直接使うことは想定しない。
///
/// Spec: spec/module/character.md

#include "ergo/character/move_source.h"

#include <vector>

namespace ergo::character::detail {

class MoveManager {
public:
    void add(IMoveSource* source);
    void remove(IMoveSource* source);

    /// 最高優先度 (>0) の source を再評価し、変化があれば lifecycle フックを
    /// 発火、保持中の source に on_move_update_highest(dt) を流す。
    void update_highest(float dt);

    /// 採用 source の速度を確定する (不在なら零ベクトル)。
    void calculate_velocity();

    const Vec3& velocity() const { return velocity_; }
    float current_speed() const { return velocity_.length(); }
    IMoveSource* current() const { return current_; }

private:
    std::vector<IMoveSource*> sources_;
    IMoveSource* current_ = nullptr;
    Vec3 velocity_{};
};

} // namespace ergo::character::detail
