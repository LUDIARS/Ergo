#pragma once

/// ergo::physics2d::World — 2D rigid body simulation world.
///
/// - N² AABB broadphase (suitable for dozens of bodies)
/// - SAT narrowphase (circle-circle, circle-polygon, polygon-polygon)
/// - Sequential impulse solver with Baumgarte position correction
/// - Semi-implicit Euler integration
/// - Contact event tracking (Begin/Stay/End)
///
/// Spec: spec/module/physics2d.md

#include "ergo/physics2d/body.h"
#include "ergo/physics2d/contact.h"
#include "ergo/math/pool.h"
#include "ergo/math/vec.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace ergo::physics2d {

/// Maximum number of bodies in a World instance.
constexpr std::size_t WORLD_MAX_BODIES = 1024;

class World {
public:
    explicit World(ergo::math::Vec<2, float> gravity = {{0.0f, -9.81f}});
    ~World() = default;

    // Move-only (ObjectPool is move-only)
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = default;
    World& operator=(World&&) = default;

    /// Create a body with the given definition and shape.
    /// Returns INVALID_BODY if the pool is full.
    BodyHandle create_body(const BodyDef& def, const Shape& shape);

    /// Destroy a body. After this call the handle is invalid.
    void destroy_body(BodyHandle handle);

    /// Advance the simulation by dt seconds.
    /// velocity_iterations: solver iterations for impulse.
    /// position_iterations: Baumgarte correction passes.
    void step(float dt,
              int velocity_iterations = 8,
              int position_iterations = 3);

    /// Access a body by handle. Returns nullptr if invalid.
    Body* get_body(BodyHandle handle);
    const Body* get_body(BodyHandle handle) const;

    /// Iterate all living bodies. Fn signature: void(BodyHandle, Body&)
    template <typename Fn>
    void for_each_body(Fn&& fn) {
        for (std::size_t i = 0; i < WORLD_MAX_BODIES; ++i) {
            if (pool_.alive(i)) {
                fn(static_cast<BodyHandle>(i), pool_.get(i));
            }
        }
    }

    /// Contact events produced during the last step().
    const std::vector<ContactEvent>& get_contact_events() const {
        return contact_events_;
    }

private:
    ergo::math::Vec<2, float>        gravity_;
    ergo::math::ObjectPool<Body>     pool_;

    // Handles currently alive (for broadphase iteration)
    std::vector<BodyHandle>          alive_handles_;

    // Contact tracking
    using ContactPair = std::pair<BodyHandle, BodyHandle>;
    std::set<ContactPair>            active_contacts_;   ///< contacts from previous step
    std::vector<ContactEvent>        contact_events_;    ///< events for current step

    // Internal steps
    void integrate(float dt);
    void broadphase_and_solve(float dt, int vel_iter, int pos_iter);
};

}  // namespace ergo::physics2d
