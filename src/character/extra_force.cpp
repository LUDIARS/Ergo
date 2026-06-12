#include "ergo/character/effects/extra_force.h"

namespace ergo::character {

void ExtraForce::on_update(float dt) {
    if (damping_ <= 0.0f) return;
    float k = 1.0f - damping_ * dt;
    if (k < 0.0f) k = 0.0f;
    velocity_ = velocity_ * k;
}

} // namespace ergo::character
