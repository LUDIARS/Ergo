#include "ergo/physics2d/physics2d.h"
#include "gtest/gtest.h"

#include <cmath>

using namespace ergo::physics2d;

// Test 1: Free fall - body falls under gravity
TEST(Physics2D, FreeFall) {
    World world(ergo::math::Vec<2,float>{{0.0f, -10.0f}});
    BodyDef def;
    def.type = BodyType::Dynamic;
    def.position = {{0.0f, 10.0f}};
    auto h = world.create_body(def, make_circle_shape(0.5f));
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; i++) world.step(dt);
    float t = 60 * dt; // 1 second
    float expected_y = 10.0f - 0.5f * 10.0f * t * t;
    EXPECT_NEAR(world.get_body(h)->position.data[1], expected_y, 0.5f);
}

// Test 2: Circle lands on static floor
TEST(Physics2D, LandOnFloor) {
    World world(ergo::math::Vec<2,float>{{0.0f, -10.0f}});
    // Static floor
    BodyDef floor_def;
    floor_def.type = BodyType::Static;
    floor_def.position = {{0.0f, 0.0f}};
    world.create_body(floor_def, make_box_shape(10.0f, 0.5f));
    // Dynamic circle above floor
    BodyDef ball_def;
    ball_def.type = BodyType::Dynamic;
    ball_def.position = {{0.0f, 5.0f}};
    ball_def.restitution = 0.0f;
    auto bh = world.create_body(ball_def, make_circle_shape(0.5f));
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 300; i++) world.step(dt);
    auto* b = world.get_body(bh);
    EXPECT_NEAR(b->position.data[1], 1.0f, 0.1f); // floor top at 0.5, circle radius 0.5 -> y~=1
    EXPECT_NEAR(b->linear_velocity.data[1], 0.0f, 0.5f);
}

// Test 3: Two overlapping circles repel
TEST(Physics2D, CirclesRepel) {
    World world(ergo::math::Vec<2,float>{{0.0f, 0.0f}});
    BodyDef a;
    a.position = {{0.0f, 0.0f}};
    a.restitution = 0.0f;
    BodyDef b_def;
    b_def.position = {{0.5f, 0.0f}};
    b_def.restitution = 0.0f;
    auto ha = world.create_body(a, make_circle_shape(0.5f));
    auto hb = world.create_body(b_def, make_circle_shape(0.5f));
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; i++) world.step(dt);
    auto* ba = world.get_body(ha);
    auto* bb = world.get_body(hb);
    float dx = bb->position.data[0] - ba->position.data[0];
    EXPECT_GT(dx, 0.9f); // they should have separated
}

// Test 4: Restitution bounce
TEST(Physics2D, Restitution) {
    World world(ergo::math::Vec<2,float>{{0.0f, -10.0f}});
    BodyDef floor_def;
    floor_def.type = BodyType::Static;
    floor_def.position = {{0.0f, 0.0f}};
    world.create_body(floor_def, make_box_shape(10.0f, 0.5f));
    BodyDef ball;
    ball.position = {{0.0f, 3.0f}};
    ball.restitution = 1.0f;
    ball.friction = 0.0f;
    ball.linear_velocity = {{0.0f, -5.0f}};
    auto bh = world.create_body(ball, make_circle_shape(0.5f));
    float dt = 1.0f / 60.0f;
    // Step until it hits floor (few frames)
    for (int i = 0; i < 30; i++) world.step(dt);
    // After bounce, velocity should be positive (upward)
    auto* b = world.get_body(bh);
    EXPECT_GT(b->linear_velocity.data[1], 0.0f);
}

// Test 5: Friction + linear damping decelerates sliding body.
// linear_damping=1.5 is an explicit per-body setting that models surface drag
// (e.g. grass, sticky floor) — this replaces the old global hardcode hack.
TEST(Physics2D, Friction) {
    World world(ergo::math::Vec<2,float>{{0.0f, -10.0f}});
    BodyDef floor_def;
    floor_def.type = BodyType::Static;
    floor_def.position = {{0.0f, 0.0f}};
    world.create_body(floor_def, make_box_shape(20.0f, 0.5f));
    BodyDef ball;
    ball.position = {{0.0f, 1.0f}};
    ball.restitution = 0.0f;
    ball.friction = 0.8f;
    ball.linear_damping = 1.5f;  // explicit per-body drag (replaces global hardcode)
    ball.linear_velocity = {{5.0f, 0.0f}};
    auto bh = world.create_body(ball, make_circle_shape(0.5f));
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 180; i++) world.step(dt);
    auto* b = world.get_body(bh);
    EXPECT_LT(std::abs(b->linear_velocity.data[0]), 1.0f); // should have decelerated
}

// Test 6: Contact begin event
TEST(Physics2D, ContactBeginEvent) {
    World world(ergo::math::Vec<2,float>{{0.0f, 0.0f}});
    BodyDef a;
    a.position = {{0.0f, 0.0f}};
    a.user_data = 42;
    BodyDef b_def;
    b_def.position = {{3.0f, 0.0f}};
    b_def.user_data = 99;
    b_def.linear_velocity = {{-5.0f, 0.0f}};
    world.create_body(a, make_circle_shape(0.5f));
    world.create_body(b_def, make_circle_shape(0.5f));
    float dt = 1.0f / 60.0f;
    bool found_begin = false;
    for (int i = 0; i < 60; i++) {
        world.step(dt);
        for (auto& ev : world.get_contact_events()) {
            if (ev.state == ContactState::Begin) {
                bool match = (ev.user_data_a == 42 && ev.user_data_b == 99) ||
                             (ev.user_data_a == 99 && ev.user_data_b == 42);
                if (match) found_begin = true;
            }
        }
    }
    EXPECT_TRUE(found_begin);
}

// Test 7: Stack stability - circles stacked don't explode
TEST(Physics2D, StackStability) {
    World world(ergo::math::Vec<2,float>{{0.0f, -10.0f}});
    BodyDef floor_def;
    floor_def.type = BodyType::Static;
    floor_def.position = {{0.0f, 0.0f}};
    world.create_body(floor_def, make_box_shape(10.0f, 0.5f));
    for (int i = 0; i < 5; i++) {
        BodyDef ball;
        ball.position = {{0.0f, 1.5f + i * 1.2f}};
        ball.restitution = 0.1f;
        world.create_body(ball, make_circle_shape(0.5f));
    }
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 300; i++) world.step(dt);
    // All bodies should be at finite positions above floor
    world.for_each_body([](BodyHandle, Body& b) {
        if (b.type == BodyType::Dynamic) {
            EXPECT_GT(b.position.data[1], -5.0f);
            EXPECT_LT(std::abs(b.position.data[0]), 50.0f);
        }
    });
}
