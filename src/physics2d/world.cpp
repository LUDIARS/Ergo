#include "ergo/physics2d/world.h"
#include "ergo/physics2d/contact.h"
#include "ergo/physics2d/body.h"

#include <algorithm>
#include <cmath>

// Forward declarations of functions defined in other compilation units
namespace ergo::physics2d {
    bool collide(const Body& a, const Body& b, Manifold& m);
    void solve_velocity(Manifold& m, Body& ba, Body& bb);
    void solve_position(Manifold& m, Body& ba, Body& bb);
}

namespace ergo::physics2d {

// ---------------------------------------------------------------------------
// AABB helpers for broadphase
// ---------------------------------------------------------------------------

struct AABB {
    float min_x, min_y, max_x, max_y;
};

static AABB compute_aabb(const Body& body) {
    AABB box{};
    if (body.shape.type == ShapeType::Circle) {
        float r = body.shape.circle.radius;
        box.min_x = body.position.data[0] - r;
        box.min_y = body.position.data[1] - r;
        box.max_x = body.position.data[0] + r;
        box.max_y = body.position.data[1] + r;
    } else {
        // Compute AABB from rotated polygon
        float min_x =  1e30f, min_y =  1e30f;
        float max_x = -1e30f, max_y = -1e30f;
        const Polygon& poly = body.shape.polygon;
        float c = std::cos(body.angle);
        float s = std::sin(body.angle);
        for (int i = 0; i < poly.count; ++i) {
            float wx = body.position.data[0] + c*poly.verts[i].data[0] - s*poly.verts[i].data[1];
            float wy = body.position.data[1] + s*poly.verts[i].data[0] + c*poly.verts[i].data[1];
            if (wx < min_x) min_x = wx;
            if (wy < min_y) min_y = wy;
            if (wx > max_x) max_x = wx;
            if (wy > max_y) max_y = wy;
        }
        box.min_x = min_x; box.min_y = min_y;
        box.max_x = max_x; box.max_y = max_y;
    }
    return box;
}

static bool aabb_overlap(const AABB& a, const AABB& b) noexcept {
    return !(a.max_x < b.min_x || b.max_x < a.min_x ||
             a.max_y < b.min_y || b.max_y < a.min_y);
}

// ---------------------------------------------------------------------------
// World implementation
// ---------------------------------------------------------------------------

World::World(ergo::math::Vec<2, float> gravity)
    : gravity_(gravity)
    , pool_(WORLD_MAX_BODIES)
{}

BodyHandle World::create_body(const BodyDef& def, const Shape& shape) {
    // Allocate slot in pool
    auto pool_handle = pool_.create();
    if (pool_handle == ergo::math::ObjectPool<Body>::invalid_handle) {
        return INVALID_BODY;
    }

    Body& body = pool_.get(pool_handle);
    body = Body{};
    body.position         = def.position;
    body.angle            = def.angle;
    body.linear_velocity  = def.linear_velocity;
    body.angular_velocity = def.angular_velocity;
    body.restitution      = def.restitution;
    body.friction         = def.friction;
    body.linear_damping   = def.linear_damping;
    body.angular_damping  = def.angular_damping;
    body.user_data        = def.user_data;
    body.type             = def.type;
    body.shape            = shape;

    compute_mass_and_inertia(body, def.density);

    BodyHandle h = static_cast<BodyHandle>(pool_handle);
    alive_handles_.push_back(h);
    return h;
}

void World::destroy_body(BodyHandle handle) {
    if (handle == INVALID_BODY || !pool_.alive(handle)) return;
    pool_.destroy(static_cast<std::size_t>(handle));
    alive_handles_.erase(
        std::remove(alive_handles_.begin(), alive_handles_.end(), handle),
        alive_handles_.end());
    // Remove from active contacts (std::set does not support remove_if)
    for (auto it = active_contacts_.begin(); it != active_contacts_.end(); ) {
        if (it->first == handle || it->second == handle)
            it = active_contacts_.erase(it);
        else
            ++it;
    }
}

Body* World::get_body(BodyHandle handle) {
    if (handle == INVALID_BODY || !pool_.alive(static_cast<std::size_t>(handle)))
        return nullptr;
    return &pool_.get(static_cast<std::size_t>(handle));
}

const Body* World::get_body(BodyHandle handle) const {
    if (handle == INVALID_BODY || !pool_.alive(static_cast<std::size_t>(handle)))
        return nullptr;
    return &pool_.get(static_cast<std::size_t>(handle));
}

void World::integrate(float dt) {
    for (BodyHandle h : alive_handles_) {
        Body& body = pool_.get(static_cast<std::size_t>(h));
        if (body.type != BodyType::Dynamic) continue;

        // Semi-implicit Euler: integrate velocity first, then position.
        body.linear_velocity  += gravity_ * dt;

        // Per-body damping (frame-rate independent, Box2D formula).
        // Applied after gravity but before position integration, matching Box2D.
        body.linear_velocity  *= 1.0f / (1.0f + dt * body.linear_damping);
        body.angular_velocity *= 1.0f / (1.0f + dt * body.angular_damping);

        body.position         += body.linear_velocity * dt;
        body.angle            += body.angular_velocity * dt;
    }
}

void World::broadphase_and_solve(float dt, int vel_iter, int pos_iter) {
    // Collect contacts from narrowphase
    std::vector<Manifold> manifolds;

    int n = static_cast<int>(alive_handles_.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            BodyHandle ha = alive_handles_[i];
            BodyHandle hb = alive_handles_[j];

            Body& ba = pool_.get(static_cast<std::size_t>(ha));
            Body& bb = pool_.get(static_cast<std::size_t>(hb));

            // Skip static-static pairs
            if (ba.type == BodyType::Static && bb.type == BodyType::Static) continue;

            // Broadphase AABB check
            AABB aabb_a = compute_aabb(ba);
            AABB aabb_b = compute_aabb(bb);
            if (!aabb_overlap(aabb_a, aabb_b)) continue;

            // Narrowphase
            Manifold m{};
            m.body_a = ha;
            m.body_b = hb;
            if (collide(ba, bb, m)) {
                manifolds.push_back(m);
            }
        }
    }

    // Track contact events
    std::set<ContactPair> current_contacts;
    contact_events_.clear();

    for (auto& m : manifolds) {
        ContactPair pair = { std::min(m.body_a, m.body_b), std::max(m.body_a, m.body_b) };
        current_contacts.insert(pair);

        Body& ba = pool_.get(static_cast<std::size_t>(m.body_a));
        Body& bb = pool_.get(static_cast<std::size_t>(m.body_b));

        ContactEvent ev{};
        ev.a = m.body_a;
        ev.b = m.body_b;
        ev.user_data_a = ba.user_data;
        ev.user_data_b = bb.user_data;

        if (active_contacts_.count(pair) == 0) {
            ev.state = ContactState::Begin;
        } else {
            ev.state = ContactState::Stay;
        }
        contact_events_.push_back(ev);
    }

    // End events for contacts that no longer exist
    for (const auto& pair : active_contacts_) {
        if (current_contacts.count(pair) == 0) {
            Body* ba = get_body(pair.first);
            Body* bb = get_body(pair.second);
            if (ba && bb) {
                ContactEvent ev{};
                ev.a = pair.first;
                ev.b = pair.second;
                ev.user_data_a = ba->user_data;
                ev.user_data_b = bb->user_data;
                ev.state = ContactState::End;
                contact_events_.push_back(ev);
            }
        }
    }

    active_contacts_ = current_contacts;

    // Velocity solver iterations
    for (int iter = 0; iter < vel_iter; ++iter) {
        for (auto& m : manifolds) {
            Body& ba = pool_.get(static_cast<std::size_t>(m.body_a));
            Body& bb = pool_.get(static_cast<std::size_t>(m.body_b));
            solve_velocity(m, ba, bb);
        }
    }

    // Position correction iterations
    for (int iter = 0; iter < pos_iter; ++iter) {
        for (auto& m : manifolds) {
            Body& ba = pool_.get(static_cast<std::size_t>(m.body_a));
            Body& bb = pool_.get(static_cast<std::size_t>(m.body_b));
            solve_position(m, ba, bb);
        }
    }

}

void World::step(float dt, int velocity_iterations, int position_iterations) {
    integrate(dt);
    broadphase_and_solve(dt, velocity_iterations, position_iterations);
}

}  // namespace ergo::physics2d
