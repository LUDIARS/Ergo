#include "ergo/character/effects/gravity.h"

namespace ergo::character {

void Gravity::add_upward_velocity(float units_per_second) {
    velocity_y_ = units_per_second;
    if (grounded_) {
        grounded_ = false;
        if (on_leave_) on_leave_();
    }
}

void Gravity::on_update(float dt) {
    const bool on_ground = ground_ && ground_->is_on_ground();

    if (on_ground && velocity_y_ <= 0.0f) {
        // 着地: 落下を止める。上昇中 (ジャンプ直後) は接地扱いにしない。
        if (!grounded_) {
            const float impact = velocity_y_;
            velocity_y_ = 0.0f;
            grounded_   = true;
            if (on_land_) on_land_(impact);
        }
        return;
    }

    if (grounded_) {
        grounded_ = false;
        if (on_leave_) on_leave_();
    }
    velocity_y_ -= gravity_ * scale_ * dt;
}

} // namespace ergo::character
