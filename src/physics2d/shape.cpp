#include "ergo/physics2d/shape.h"

namespace ergo::physics2d {

Polygon make_box(float half_w, float half_h) {
    Polygon p{};
    p.count = 4;
    // CCW order
    p.verts[0].data[0] = -half_w; p.verts[0].data[1] = -half_h;
    p.verts[1].data[0] =  half_w; p.verts[1].data[1] = -half_h;
    p.verts[2].data[0] =  half_w; p.verts[2].data[1] =  half_h;
    p.verts[3].data[0] = -half_w; p.verts[3].data[1] =  half_h;
    return p;
}

Shape make_circle_shape(float radius) {
    Shape s{};
    s.type = ShapeType::Circle;
    s.circle.radius = radius;
    return s;
}

Shape make_box_shape(float half_w, float half_h) {
    Shape s{};
    s.type = ShapeType::Polygon;
    s.polygon = make_box(half_w, half_h);
    return s;
}

}  // namespace ergo::physics2d
