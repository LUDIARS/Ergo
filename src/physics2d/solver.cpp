#include "ergo/physics2d/contact.h"
#include "ergo/physics2d/body.h"

#include <cmath>
#include <algorithm>

namespace ergo::physics2d {

// ---------------------------------------------------------------------------
// 2D cross products for solver
// ---------------------------------------------------------------------------

/// Cross product of 2D vector with scalar: returns perpendicular scaled vector
static inline ergo::math::Vec<2,float> cross_sv(float s, const ergo::math::Vec<2,float>& v) noexcept {
    ergo::math::Vec<2,float> r{};
    r.data[0] = -s * v.data[1];
    r.data[1] =  s * v.data[0];
    return r;
}

/// 2D cross product (scalar result): a × b = ax*by - ay*bx
static inline float cross_vv(const ergo::math::Vec<2,float>& a,
                              const ergo::math::Vec<2,float>& b) noexcept {
    return a.data[0]*b.data[1] - a.data[1]*b.data[0];
}

// ---------------------------------------------------------------------------
// Sequential impulse solver
// ---------------------------------------------------------------------------

void solve_velocity(Manifold& m, Body& ba, Body& bb) {
    // Baumgarte bias (position correction via velocity)
    const float beta = 0.2f;          // bias factor
    const float slop = 0.01f;         // penetration allowance

    for (int ci = 0; ci < m.contact_count; ++ci) {
        const ergo::math::Vec<2,float>& cp = m.contact_points[ci];

        // Arm vectors from body centres to contact point
        ergo::math::Vec<2,float> ra = cp - ba.position;
        ergo::math::Vec<2,float> rb = cp - bb.position;

        // Relative velocity at contact point
        // vA + wA × rA
        ergo::math::Vec<2,float> va = ba.linear_velocity + cross_sv(ba.angular_velocity, ra);
        ergo::math::Vec<2,float> vb = bb.linear_velocity + cross_sv(bb.angular_velocity, rb);
        ergo::math::Vec<2,float> rel_vel = vb - va;

        float vel_along_normal = rel_vel.dot(m.normal);

        // Only resolve if bodies are approaching
        if (vel_along_normal > 0.0f) continue;

        // Effective mass denominator
        float ra_cross_n = cross_vv(ra, m.normal);
        float rb_cross_n = cross_vv(rb, m.normal);
        float inv_mass_sum = ba.inv_mass + bb.inv_mass
            + (ra_cross_n * ra_cross_n) * ba.inv_inertia
            + (rb_cross_n * rb_cross_n) * bb.inv_inertia;

        if (inv_mass_sum < 1e-10f) continue;

        // Normal impulse
        float e = m.restitution;
        float j = -(1.0f + e) * vel_along_normal / inv_mass_sum;

        // Clamp accumulated impulse
        float old_acc = m.accumulated_normal[ci];
        m.accumulated_normal[ci] = std::max(0.0f, old_acc + j);
        float lambda = m.accumulated_normal[ci] - old_acc;

        ergo::math::Vec<2,float> impulse = m.normal * lambda;

        ba.linear_velocity  -= impulse * ba.inv_mass;
        ba.angular_velocity -= cross_vv(ra, impulse) * ba.inv_inertia;
        bb.linear_velocity  += impulse * bb.inv_mass;
        bb.angular_velocity += cross_vv(rb, impulse) * bb.inv_inertia;

        // Friction impulse (Coulomb)
        // Recompute relative velocity after normal impulse
        va = ba.linear_velocity + cross_sv(ba.angular_velocity, ra);
        vb = bb.linear_velocity + cross_sv(bb.angular_velocity, rb);
        rel_vel = vb - va;

        ergo::math::Vec<2,float> tangent{};
        float vel_t = rel_vel.data[0]*(-m.normal.data[1]) + rel_vel.data[1]*m.normal.data[0];
        tangent.data[0] = -m.normal.data[1];
        tangent.data[1] =  m.normal.data[0];
        // tangent direction opposing slip
        if (vel_t < 0.0f) tangent = tangent * -1.0f;

        float ra_cross_t = cross_vv(ra, tangent);
        float rb_cross_t = cross_vv(rb, tangent);
        float inv_mass_t = ba.inv_mass + bb.inv_mass
            + (ra_cross_t * ra_cross_t) * ba.inv_inertia
            + (rb_cross_t * rb_cross_t) * bb.inv_inertia;

        if (inv_mass_t < 1e-10f) continue;

        float jt = -std::abs(vel_t) / inv_mass_t;

        // Coulomb clamp
        float mu = m.friction;
        float max_friction = mu * m.accumulated_normal[ci];
        float old_acc_t = m.accumulated_tangent[ci];
        m.accumulated_tangent[ci] = std::max(-max_friction, std::min(old_acc_t + jt, max_friction));
        float lambda_t = m.accumulated_tangent[ci] - old_acc_t;

        ergo::math::Vec<2,float> friction_impulse = tangent * lambda_t;

        ba.linear_velocity  -= friction_impulse * ba.inv_mass;
        ba.angular_velocity -= cross_vv(ra, friction_impulse) * ba.inv_inertia;
        bb.linear_velocity  += friction_impulse * bb.inv_mass;
        bb.angular_velocity += cross_vv(rb, friction_impulse) * bb.inv_inertia;
    }
}

void solve_position(Manifold& m, Body& ba, Body& bb) {
    const float beta = 0.2f;
    const float slop = 0.01f;

    float correction_depth = std::max(m.penetration - slop, 0.0f);
    float inv_mass_sum = ba.inv_mass + bb.inv_mass;
    if (inv_mass_sum < 1e-10f) return;

    ergo::math::Vec<2,float> correction =
        m.normal * (correction_depth * beta / inv_mass_sum);

    ba.position -= correction * ba.inv_mass;
    bb.position += correction * bb.inv_mass;
}

}  // namespace ergo::physics2d
