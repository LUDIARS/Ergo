#pragma once

/// ergo::physics2d::Shape — shape types for 2D rigid body physics.
///
/// Shapes are plain data (no vtable). Shape variant uses an enum tag
/// so the compiler can enforce exhaustive coverage.
///
/// Spec: spec/module/physics2d.md

#include "ergo/math/vec.h"
#include <cstdint>

namespace ergo::physics2d {

// ---------------------------------------------------------------------------
// Polygon vertex limit
// ---------------------------------------------------------------------------
constexpr int MAX_POLYGON_VERTS = 8;

// ---------------------------------------------------------------------------
// Primitive shapes
// ---------------------------------------------------------------------------

struct Circle {
    float radius = 0.5f;
};

/// Convex polygon with up to MAX_POLYGON_VERTS vertices in CCW order.
struct Polygon {
    ergo::math::Vec<2, float> verts[MAX_POLYGON_VERTS]{};
    int count = 0;
};

/// Build a CCW axis-aligned box centred at origin.
Polygon make_box(float half_w, float half_h);

// ---------------------------------------------------------------------------
// Shape variant (enum tag, no vtable)
// ---------------------------------------------------------------------------

enum class ShapeType { Circle, Polygon };

struct Shape {
    ShapeType type = ShapeType::Circle;
    union {
        Circle  circle;
        Polygon polygon;
    };

    Shape() : type(ShapeType::Circle), circle{} {}
};

Shape make_circle_shape(float radius);
Shape make_box_shape(float half_w, float half_h);

}  // namespace ergo::physics2d
