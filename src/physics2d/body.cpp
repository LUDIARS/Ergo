#include "ergo/physics2d/body.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ergo::physics2d {

void compute_mass_and_inertia(Body& body, float density) {
    // Static and kinematic bodies have infinite mass
    if (body.type == BodyType::Static || body.type == BodyType::Kinematic) {
        body.mass       = 0.0f;
        body.inv_mass   = 0.0f;
        body.inertia    = 0.0f;
        body.inv_inertia = 0.0f;
        return;
    }

    float area    = 0.0f;
    float inertia = 0.0f;

    if (body.shape.type == ShapeType::Circle) {
        float r = body.shape.circle.radius;
        area    = static_cast<float>(M_PI) * r * r;
        // I = 0.5 * m * r^2  (moment of inertia for solid disk)
        float mass_local = density * area;
        inertia = 0.5f * mass_local * r * r;
        body.mass    = mass_local;
    } else {
        // Polygon: shoelace formula for area, then parallel-axis theorem for inertia
        const Polygon& poly = body.shape.polygon;
        // Compute area using shoelace
        float signed_area = 0.0f;
        ergo::math::Vec<2, float> centroid{};
        for (int i = 0; i < poly.count; ++i) {
            int j = (i + 1) % poly.count;
            const auto& vi = poly.verts[i];
            const auto& vj = poly.verts[j];
            float cross = vi.data[0] * vj.data[1] - vj.data[0] * vi.data[1];
            signed_area += cross;
            centroid.data[0] += (vi.data[0] + vj.data[0]) * cross;
            centroid.data[1] += (vi.data[1] + vj.data[1]) * cross;
        }
        signed_area *= 0.5f;
        if (signed_area < 0.0f) signed_area = -signed_area;  // ensure positive
        area = signed_area;

        float denom = (6.0f * signed_area);
        if (denom != 0.0f) {
            centroid.data[0] /= denom;
            centroid.data[1] /= denom;
        }

        // Compute inertia using the polygon inertia formula (triangle fan from origin)
        float I = 0.0f;
        for (int i = 0; i < poly.count; ++i) {
            int j = (i + 1) % poly.count;
            const auto& a = poly.verts[i];
            const auto& b = poly.verts[j];
            float cross = a.data[0] * b.data[1] - b.data[0] * a.data[1];
            if (cross < 0.0f) cross = -cross;
            float dot_aa = a.data[0]*a.data[0] + a.data[1]*a.data[1];
            float dot_ab = a.data[0]*b.data[0] + a.data[1]*b.data[1];
            float dot_bb = b.data[0]*b.data[0] + b.data[1]*b.data[1];
            I += cross * (dot_aa + dot_ab + dot_bb);
        }
        I *= density / 12.0f;

        float mass_local = density * area;
        body.mass = mass_local;
        inertia   = I;
    }

    body.inv_mass     = (body.mass > 0.0f) ? 1.0f / body.mass : 0.0f;
    body.inertia      = inertia;
    body.inv_inertia  = (inertia > 0.0f) ? 1.0f / inertia : 0.0f;
}

}  // namespace ergo::physics2d
