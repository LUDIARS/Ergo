#include "ergo/physics2d/contact.h"
#include "ergo/physics2d/body.h"
#include "ergo/math/mat.h"

#include <cmath>
#include <limits>
#include <algorithm>

namespace ergo::physics2d {

// ---------------------------------------------------------------------------
// 2D math helpers (internal)
// ---------------------------------------------------------------------------

static inline float vec2_cross(const ergo::math::Vec<2,float>& a,
                                const ergo::math::Vec<2,float>& b) noexcept {
    return a.data[0]*b.data[1] - a.data[1]*b.data[0];
}

static inline ergo::math::Vec<2,float> vec2_perp(const ergo::math::Vec<2,float>& v) noexcept {
    ergo::math::Vec<2,float> r{};
    r.data[0] = -v.data[1];
    r.data[1] =  v.data[0];
    return r;
}

static inline ergo::math::Vec<2,float> rotate_vec(const ergo::math::Vec<2,float>& v, float angle) noexcept {
    float c = std::cos(angle), s = std::sin(angle);
    ergo::math::Vec<2,float> r{};
    r.data[0] = c*v.data[0] - s*v.data[1];
    r.data[1] = s*v.data[0] + c*v.data[1];
    return r;
}

// ---------------------------------------------------------------------------
// Build world-space polygon vertices
// ---------------------------------------------------------------------------
static void build_world_verts(const Body& body,
                               ergo::math::Vec<2,float> out[MAX_POLYGON_VERTS],
                               int& count) {
    const Polygon& poly = body.shape.polygon;
    count = poly.count;
    for (int i = 0; i < poly.count; ++i) {
        out[i] = body.position + rotate_vec(poly.verts[i], body.angle);
    }
}

// ---------------------------------------------------------------------------
// SAT helper: project polygon onto axis, returns [min, max]
// ---------------------------------------------------------------------------
static void project_polygon(const ergo::math::Vec<2,float>* verts, int count,
                             const ergo::math::Vec<2,float>& axis,
                             float& out_min, float& out_max) {
    out_min = out_max = verts[0].dot(axis);
    for (int i = 1; i < count; ++i) {
        float p = verts[i].dot(axis);
        if (p < out_min) out_min = p;
        if (p > out_max) out_max = p;
    }
}

// ---------------------------------------------------------------------------
// SAT for polygon-polygon
// Returns false if separated, fills manifold if overlapping.
// ---------------------------------------------------------------------------
static bool sat_polygon_polygon(
    const ergo::math::Vec<2,float>* vertsA, int countA,
    const ergo::math::Vec<2,float>* vertsB, int countB,
    ergo::math::Vec<2,float>& out_normal, float& out_penetration)
{
    out_penetration = std::numeric_limits<float>::max();
    ergo::math::Vec<2,float> best_normal{};

    // Test axes from A
    for (int i = 0; i < countA; ++i) {
        int j = (i + 1) % countA;
        ergo::math::Vec<2,float> edge = vertsA[j] - vertsA[i];
        ergo::math::Vec<2,float> axis = vec2_perp(edge).normalized();

        float minA, maxA, minB, maxB;
        project_polygon(vertsA, countA, axis, minA, maxA);
        project_polygon(vertsB, countB, axis, minB, maxB);

        if (maxA < minB || maxB < minA) return false;  // separated

        float overlap = std::min(maxA, maxB) - std::max(minA, minB);
        if (overlap < out_penetration) {
            out_penetration = overlap;
            best_normal = axis;
        }
    }

    // Test axes from B
    for (int i = 0; i < countB; ++i) {
        int j = (i + 1) % countB;
        ergo::math::Vec<2,float> edge = vertsB[j] - vertsB[i];
        ergo::math::Vec<2,float> axis = vec2_perp(edge).normalized();

        float minA, maxA, minB, maxB;
        project_polygon(vertsA, countA, axis, minA, maxA);
        project_polygon(vertsB, countB, axis, minB, maxB);

        if (maxA < minB || maxB < minA) return false;

        float overlap = std::min(maxA, maxB) - std::max(minA, minB);
        if (overlap < out_penetration) {
            out_penetration = overlap;
            best_normal = axis;
        }
    }

    out_normal = best_normal;
    return true;
}

// ---------------------------------------------------------------------------
// Sutherland-Hodgman clipping helper
// ---------------------------------------------------------------------------
static int clip_segment_to_line(
    const ergo::math::Vec<2,float> in[2],
    ergo::math::Vec<2,float> out[2],
    const ergo::math::Vec<2,float>& line_normal,
    float line_c)
{
    int count = 0;
    float d0 = in[0].dot(line_normal) - line_c;
    float d1 = in[1].dot(line_normal) - line_c;

    if (d0 >= 0.0f) out[count++] = in[0];
    if (d1 >= 0.0f) out[count++] = in[1];

    if (d0 * d1 < 0.0f) {
        float t = d0 / (d0 - d1);
        ergo::math::Vec<2,float> diff = in[1] - in[0];
        out[count++] = in[0] + diff * t;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Find contact points for polygon-polygon (reference edge + Sutherland-Hodgman)
// ---------------------------------------------------------------------------
static void find_polygon_contact_points(
    const ergo::math::Vec<2,float>* vertsA, int countA,
    const ergo::math::Vec<2,float>* vertsB, int countB,
    const ergo::math::Vec<2,float>& normal,
    Manifold& m)
{
    // Find reference edge in A (most anti-parallel to normal)
    int ref_idx = 0;
    float min_dot = std::numeric_limits<float>::max();
    for (int i = 0; i < countA; ++i) {
        int j = (i + 1) % countA;
        ergo::math::Vec<2,float> edge = vertsA[j] - vertsA[i];
        ergo::math::Vec<2,float> en = edge.normalized();
        float d = std::abs(en.dot(normal));
        if (d < min_dot) {
            min_dot = d;
            ref_idx = i;
        }
    }

    ergo::math::Vec<2,float> ref_v0 = vertsA[ref_idx];
    ergo::math::Vec<2,float> ref_v1 = vertsA[(ref_idx + 1) % countA];
    ergo::math::Vec<2,float> ref_edge = (ref_v1 - ref_v0).normalized();

    // Find incident vertex in B (most in normal direction)
    int inc_idx = 0;
    float max_proj = -std::numeric_limits<float>::max();
    for (int i = 0; i < countB; ++i) {
        float p = vertsB[i].dot(normal);
        if (p > max_proj) {
            max_proj = p;
            inc_idx = i;
        }
    }
    int inc_idx2 = (inc_idx == 0) ? countB - 1 : inc_idx - 1;
    if (vertsB[(inc_idx + 1) % countB].dot(normal) > vertsB[inc_idx2].dot(normal))
        inc_idx2 = (inc_idx + 1) % countB;

    ergo::math::Vec<2,float> inc[2] = { vertsB[inc_idx], vertsB[inc_idx2] };

    // Clip incident edge against reference edge sides
    ergo::math::Vec<2,float> clipped[2];
    float side1 = ref_edge.dot(ref_v0);
    int cnt = clip_segment_to_line(inc, clipped, ref_edge, side1);
    if (cnt < 2) {
        // fallback: use nearest B vertex
        m.contact_count = 1;
        m.contact_points[0] = vertsB[inc_idx];
        return;
    }

    ergo::math::Vec<2,float> clipped2[4];
    ergo::math::Vec<2,float> neg_ref = ref_edge * -1.0f;
    float side2 = neg_ref.dot(ref_v1);
    int cnt2 = clip_segment_to_line(clipped, clipped2, neg_ref, side2);

    // Keep points behind reference face
    float ref_c = normal.dot(ref_v0);
    m.contact_count = 0;
    for (int i = 0; i < cnt2 && m.contact_count < 2; ++i) {
        if (clipped2[i].dot(normal) <= ref_c + 0.001f) {
            m.contact_points[m.contact_count++] = clipped2[i];
        }
    }
    if (m.contact_count == 0) {
        m.contact_count = 1;
        m.contact_points[0] = vertsB[inc_idx];
    }
}

// ---------------------------------------------------------------------------
// Public narrowphase functions
// ---------------------------------------------------------------------------

/// Circle vs Circle
bool collide_circle_circle(const Body& a, const Body& b, Manifold& m) {
    ergo::math::Vec<2,float> d = b.position - a.position;
    float dist_sq = d.length_sq();
    float ra = a.shape.circle.radius;
    float rb = b.shape.circle.radius;
    float sum_r = ra + rb;
    if (dist_sq >= sum_r * sum_r) return false;

    float dist = std::sqrt(dist_sq);
    m.penetration = sum_r - dist;
    if (dist < 1e-6f) {
        // Exactly overlapping: pick arbitrary normal
        m.normal = {{1.0f, 0.0f}};
    } else {
        m.normal = d / dist;
    }
    // Contact point: midpoint on the surface of A toward B
    m.contact_points[0] = a.position + m.normal * ra;
    m.contact_count = 1;
    m.restitution = (a.restitution + b.restitution) * 0.5f;
    m.friction    = std::sqrt(a.friction * b.friction);
    return true;
}

/// Circle vs Polygon (circle_body = A, poly_body = B).
/// Manifold normal convention: FROM A (circle) TOWARD B (polygon).
/// Impulse j*normal is applied as: A -= j*normal*inv_mass_A,  B += j*normal*inv_mass_B.
/// So a positive j with normal pointing INTO the polygon pushes A away from B.
bool collide_circle_polygon(const Body& circle_body, const Body& poly_body, Manifold& m, bool swapped) {
    ergo::math::Vec<2,float> world_verts[MAX_POLYGON_VERTS];
    int count = 0;
    build_world_verts(poly_body, world_verts, count);

    ergo::math::Vec<2,float> center = circle_body.position;
    float radius = circle_body.shape.circle.radius;

    // -----------------------------------------------------------------------
    // Use SAT against the polygon edges: circle is modeled as a point + radius.
    // For each edge axis, check separation. Track minimum overlap axis.
    // Also check vertex-voronoi axes for vertex-nearest cases.
    // -----------------------------------------------------------------------

    // We'll find the axis of minimum penetration using SAT on edge normals.
    // For each edge of the polygon (in world space), compute:
    //   edge_normal = outward normal (for CCW: right-hand perp = vec2_perp reversed)
    //   separation = dot(center, edge_normal) - dot(vert, edge_normal) - radius
    // If any separation > 0 → no collision.

    // For CCW winding, outward normal of edge i→j is the LEFT-perp of (j-i).
    // vec2_perp(v) = (-v.y, v.x) is the LEFT perp (CCW 90°).
    // For CCW polygon, LEFT-perp of the forward edge = INWARD normal.
    // Outward normal = RIGHT-perp = (v.y, -v.x) = -vec2_perp(v).

    float min_sep = -std::numeric_limits<float>::max();
    int best_edge = 0;

    for (int i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        ergo::math::Vec<2,float> edge = world_verts[j] - world_verts[i];
        // Outward normal for CCW polygon: right-perp
        ergo::math::Vec<2,float> axis{};
        axis.data[0] =  edge.data[1];
        axis.data[1] = -edge.data[0];
        axis = axis.normalized();

        // Signed distance from edge to circle center along outward normal
        float sep = axis.dot(center - world_verts[i]) - radius;
        if (sep > 0.0f) return false;  // separated on this axis

        if (sep > min_sep) {
            min_sep = sep;
            best_edge = i;
        }
    }

    // min_sep <= 0 means we have a collision.
    // The shallowest overlap is on best_edge.

    // Determine manifold normal = outward normal of the best edge (FROM polygon surface OUTWARD).
    // We need it to point FROM A (circle) TOWARD B (polygon) = inward = negate outward.
    int j = (best_edge + 1) % count;
    ergo::math::Vec<2,float> edge = world_verts[j] - world_verts[best_edge];
    ergo::math::Vec<2,float> outward{};
    outward.data[0] =  edge.data[1];
    outward.data[1] = -edge.data[0];
    outward = outward.normalized();

    // Normal FROM circle (A) TOWARD polygon (B): inward normal = -outward.
    // Penetration depth = -min_sep (positive value).
    ergo::math::Vec<2,float> normal_a_to_b = outward * -1.0f;
    float penetration = -min_sep;

    if (swapped) {
        // Called as (poly_body=A, circle_body=B): flip so normal is from actual A to B.
        normal_a_to_b = normal_a_to_b * -1.0f;
    }

    m.normal = normal_a_to_b;
    m.penetration = penetration;
    // Contact point: on the circle surface facing the polygon
    // = circle center - outward * radius  (outward = away from polygon)
    m.contact_points[0] = circle_body.position - outward * radius;
    m.contact_count = 1;
    m.restitution = (circle_body.restitution + poly_body.restitution) * 0.5f;
    m.friction    = std::sqrt(circle_body.friction * poly_body.friction);
    return true;
}

/// Polygon vs Polygon
bool collide_polygon_polygon(const Body& a, const Body& b, Manifold& m) {
    ergo::math::Vec<2,float> verts_a[MAX_POLYGON_VERTS];
    ergo::math::Vec<2,float> verts_b[MAX_POLYGON_VERTS];
    int count_a = 0, count_b = 0;
    build_world_verts(a, verts_a, count_a);
    build_world_verts(b, verts_b, count_b);

    ergo::math::Vec<2,float> normal{};
    float penetration = 0.0f;

    if (!sat_polygon_polygon(verts_a, count_a, verts_b, count_b, normal, penetration))
        return false;

    // Ensure normal points from A to B
    ergo::math::Vec<2,float> center_diff = b.position - a.position;
    if (normal.dot(center_diff) < 0.0f) {
        normal = normal * -1.0f;
    }

    m.normal = normal;
    m.penetration = penetration;

    // Find contact points using clipping
    find_polygon_contact_points(verts_a, count_a, verts_b, count_b, normal, m);

    m.restitution = (a.restitution + b.restitution) * 0.5f;
    m.friction    = std::sqrt(a.friction * b.friction);
    return true;
}

/// Dispatch narrowphase based on shape types.
/// Returns true and fills m if colliding.
/// m.body_a and m.body_b must be filled by the caller.
bool collide(const Body& a, const Body& b, Manifold& m) {
    if (a.shape.type == ShapeType::Circle && b.shape.type == ShapeType::Circle) {
        return collide_circle_circle(a, b, m);
    }
    if (a.shape.type == ShapeType::Circle && b.shape.type == ShapeType::Polygon) {
        return collide_circle_polygon(a, b, m, false);
    }
    if (a.shape.type == ShapeType::Polygon && b.shape.type == ShapeType::Circle) {
        // Swap roles and flip normal
        bool result = collide_circle_polygon(b, a, m, true);
        return result;
    }
    // Polygon vs Polygon
    return collide_polygon_polygon(a, b, m);
}

}  // namespace ergo::physics2d
