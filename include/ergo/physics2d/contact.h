#pragma once

/// ergo::physics2d::ContactEvent — contact event types.
///
/// Spec: spec/module/physics2d.md

#include "ergo/physics2d/body.h"
#include <cstdint>

namespace ergo::physics2d {

enum class ContactState { Begin, Stay, End };

struct ContactEvent {
    BodyHandle a, b;
    uint64_t user_data_a, user_data_b;
    ContactState state;
};

/// Internal contact manifold produced by narrowphase.
struct Manifold {
    BodyHandle body_a, body_b;

    ergo::math::Vec<2, float> normal;   ///< from A toward B
    float penetration = 0.0f;           ///< overlap depth (positive = penetrating)

    ergo::math::Vec<2, float> contact_points[2]{};
    int contact_count = 0;

    float restitution = 0.0f;
    float friction = 0.0f;

    /// Accumulated normal impulse (for warm-starting / clamping)
    float accumulated_normal[2] = {0.0f, 0.0f};
    float accumulated_tangent[2] = {0.0f, 0.0f};
};

}  // namespace ergo::physics2d
