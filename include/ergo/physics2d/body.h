#pragma once

/// ergo::physics2d::Body — rigid body data, BodyDef, BodyHandle.
///
/// Spec: spec/module/physics2d.md

#include "ergo/physics2d/shape.h"
#include "ergo/math/vec.h"

#include <cstdint>
#include <climits>

namespace ergo::physics2d {

using BodyHandle = uint32_t;
constexpr BodyHandle INVALID_BODY = UINT32_MAX;

enum class BodyType { Static, Dynamic, Kinematic };

/// Construction parameters for a body.
struct BodyDef {
    BodyType type                         = BodyType::Dynamic;
    ergo::math::Vec<2, float> position    = {};
    float angle                           = 0.0f;
    ergo::math::Vec<2, float> linear_velocity  = {};
    float angular_velocity                = 0.0f;
    float restitution                     = 0.3f;
    float friction                        = 0.5f;
    float density                         = 1.0f;
    /// Box2D-like per-body linear damping coefficient (>= 0).
    /// Applied as: v *= 1 / (1 + dt * linear_damping)
    float linear_damping                  = 0.0f;
    /// Box2D-like per-body angular damping coefficient (>= 0).
    /// Applied as: omega *= 1 / (1 + dt * angular_damping)
    float angular_damping                 = 0.0f;
    uint64_t user_data                    = 0;
};

/// Internal runtime state for one body (stored in ObjectPool slab).
struct Body {
    ergo::math::Vec<2, float> position;
    float angle;
    ergo::math::Vec<2, float> linear_velocity;
    float angular_velocity;
    float mass, inv_mass;
    float inertia, inv_inertia;
    float restitution;
    float friction;
    float linear_damping;   ///< v *= 1/(1+dt*linear_damping) per step
    float angular_damping;  ///< omega *= 1/(1+dt*angular_damping) per step
    uint64_t user_data;
    BodyType type;
    Shape shape;

    Body() = default;
};

/// Compute mass and inertia for a body given its shape and density.
/// Sets mass, inv_mass, inertia, inv_inertia in-place.
void compute_mass_and_inertia(Body& body, float density);

}  // namespace ergo::physics2d
