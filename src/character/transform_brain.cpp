#include "ergo/character/transform_brain.h"

namespace ergo::character {

TransformBrain::TransformBrain(Pose* pose) : pose_(pose) {
    if (pose_) {
        cached_position_ = pose_->position;
        cached_yaw_deg_  = pose_->yaw_deg;
    }
}

void TransformBrain::apply_position(const Vec3& total_velocity, float dt) {
    if (!pose_) return;
    Vec3 delta = total_velocity * dt;
    if (move_filter_) delta = move_filter_(pose_->position, delta);
    pose_->position += delta;
    cached_position_ = pose_->position;
}

void TransformBrain::apply_yaw(float yaw_deg) {
    if (!pose_) return;
    pose_->yaw_deg  = wrap_angle_deg(yaw_deg);
    cached_yaw_deg_ = pose_->yaw_deg;
}

void TransformBrain::set_position_directly(const Vec3& position) {
    if (!pose_) return;
    pose_->position  = position;
    cached_position_ = position;
}

void TransformBrain::set_yaw_directly(float yaw_deg) {
    if (!pose_) return;
    pose_->yaw_deg  = wrap_angle_deg(yaw_deg);
    cached_yaw_deg_ = pose_->yaw_deg;
}

} // namespace ergo::character
