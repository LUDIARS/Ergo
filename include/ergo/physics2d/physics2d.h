#pragma once

/// ergo::physics2d — 2D rigid body physics engine (Box2D-like).
///
/// Umbrella header. Include this to pull in all public API:
///   shape.h   — Circle, Polygon, Shape variant, make_*_shape helpers
///   body.h    — Body, BodyDef, BodyType, BodyHandle
///   contact.h — ContactEvent, ContactState, Manifold
///   world.h   — World class
///
/// Usage:
/// @code
///   #include "ergo/physics2d/physics2d.h"
///   using namespace ergo::physics2d;
///
///   World world({0.0f, -9.81f});
///   BodyDef def; def.position = {{0.0f, 5.0f}};
///   auto h = world.create_body(def, make_circle_shape(0.5f));
///   world.step(1.0f / 60.0f);
///   auto* b = world.get_body(h);
/// @endcode
///
/// Spec: spec/module/physics2d.md

#include "ergo/physics2d/shape.h"
#include "ergo/physics2d/body.h"
#include "ergo/physics2d/contact.h"
#include "ergo/physics2d/world.h"
