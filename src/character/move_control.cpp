#include "ergo/character/controls/move_control.h"

namespace ergo::character {

void MoveControl::set_move_input(const Vec3& input_xz) {
    input_  = {input_xz.x, 0.0f, input_xz.z};
    moving_ = input_.length_xz() > input_threshold_;
    if (moving_) {
        target_yaw_deg_ = direction_to_yaw_deg(input_);
        has_faced_      = true;
    }
}

Vec3 MoveControl::move_velocity() const {
    if (!moving_) return {};
    return input_ * speed_;
}

int MoveControl::turn_priority() const {
    // 一度も入力方向を向いていないうちは不在扱い (初期向きを壊さない)。
    return has_faced_ ? turn_priority_ : 0;
}

} // namespace ergo::character
