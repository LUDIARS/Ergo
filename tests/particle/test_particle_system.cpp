#include "ergo/particle/particle_system.h"
#include "gtest/gtest.h"

#include <thread>

using namespace ergo::particle;

TEST(ParticleSystem, EmptyOnConstruction) {
    ParticleSystem s;
    EXPECT_EQ(s.count(), 0u);
    s.update(0.016f);
    EXPECT_EQ(s.count(), 0u);
}

TEST(ParticleSystem, RateProducesAboutNPerSecond) {
    ParticleSystem s;
    ParticleEffectConfig c;
    c.emission_rate      = 100.0f;
    c.emission_max_alive = 10000;
    c.init_lifetime_min  = 5.0f;
    c.init_lifetime_max  = 5.0f;
    s.set_config(c);

    // Step in small slices to accumulate emission realistically.
    for (int i = 0; i < 60; ++i) s.update(1.0f / 60.0f);
    // Expect ~100 ± 10%.
    EXPECT_GE(s.count(), 90u);
    EXPECT_LE(s.count(), 110u);
}

TEST(ParticleSystem, RespectsMaxAlive) {
    ParticleSystem s;
    ParticleEffectConfig c;
    c.emission_rate      = 1000.0f;
    c.emission_max_alive = 50;
    c.init_lifetime_min  = 10.0f;
    c.init_lifetime_max  = 10.0f;
    s.set_config(c);
    for (int i = 0; i < 60; ++i) s.update(1.0f / 60.0f);
    EXPECT_EQ(s.count(), 50u);
}

TEST(ParticleSystem, ParticlesExpire) {
    ParticleSystem s;
    ParticleEffectConfig c;
    c.emission_rate      = 0.0f;
    c.emission_max_alive = 100;
    c.init_lifetime_min  = 0.5f;
    c.init_lifetime_max  = 0.5f;
    s.set_config(c);
    s.update(0.0f); // pull cfg in
    s.burst(20);
    EXPECT_EQ(s.count(), 20u);

    // Advance past the lifetime
    for (int i = 0; i < 10; ++i) s.update(0.1f);
    EXPECT_EQ(s.count(), 0u);
}

TEST(ParticleSystem, GravityAccelerates) {
    ParticleSystem s;
    ParticleEffectConfig c;
    c.emission_rate         = 0.0f;
    c.emission_max_alive    = 100;
    c.init_lifetime_min     = 5.0f;
    c.init_lifetime_max     = 5.0f;
    c.init_speed_min        = 0.0f;
    c.init_speed_max        = 0.0f;
    c.init_position_radius  = 0.0f;
    c.life_velocity_damping = 1.0f; // no damping
    c.gravity               = {0.0f, 100.0f};
    s.set_config(c);
    s.update(0.0f);
    s.burst(1);

    // After 1 second under gravity 100, |y| should be ~50 (∫t·g·dt).
    for (int i = 0; i < 100; ++i) s.update(0.01f);
    ASSERT_EQ(s.count(), 1u);
    const auto& p = s.instances()[0];
    EXPECT_NEAR(p.pos[1], 50.0f, 5.0f);
}

TEST(ParticleSystem, DampingDecaysVelocity) {
    ParticleSystem s;
    ParticleEffectConfig c;
    c.emission_rate         = 0.0f;
    c.emission_max_alive    = 100;
    c.init_lifetime_min     = 5.0f;
    c.init_lifetime_max     = 5.0f;
    c.init_speed_min        = 100.0f;
    c.init_speed_max        = 100.0f;
    c.init_velocity_angle_deg     = 0.0f;
    c.init_velocity_spread_deg    = 0.0f;
    c.init_position_radius  = 0.0f;
    c.life_velocity_damping = 0.25f; // very strong damping
    c.gravity               = {0.0f, 0.0f};
    s.set_config(c);
    s.update(0.0f);
    s.burst(1);

    // After 1 second the velocity should have decayed massively.
    for (int i = 0; i < 100; ++i) s.update(0.01f);
    ASSERT_EQ(s.count(), 1u);
    // Position can't have advanced much (initial 100 in X, decaying to ~25 of distance).
    EXPECT_LT(s.instances()[0].pos[0], 90.0f);
}

TEST(ParticleSystem, ColorInterpolatesOverLife) {
    ParticleSystem s;
    ParticleEffectConfig c;
    c.emission_rate      = 0.0f;
    c.emission_max_alive = 100;
    c.init_lifetime_min  = 1.0f;
    c.init_lifetime_max  = 1.0f;
    c.init_color         = {1, 0, 0, 1};
    c.life_color_start   = {1, 0, 0, 1};
    c.life_color_end     = {0, 0, 1, 0};
    s.set_config(c);
    s.update(0.0f);
    s.burst(1);

    // Right after spawn -> red (color_start).
    s.update(0.001f);
    ASSERT_EQ(s.count(), 1u);
    EXPECT_NEAR(s.instances()[0].color[0], 1.0f, 0.05f);
    EXPECT_NEAR(s.instances()[0].color[2], 0.0f, 0.05f);

    // Halfway -> magenta-ish.
    for (int i = 0; i < 49; ++i) s.update(0.01f);
    EXPECT_NEAR(s.instances()[0].color[0], 0.5f, 0.1f);
    EXPECT_NEAR(s.instances()[0].color[2], 0.5f, 0.1f);
}

TEST(ParticleSystem, SetConfigFromOtherThreadDoesNotCrash) {
    ParticleSystem s;
    std::atomic<bool> stop{false};
    std::thread writer([&]{
        ParticleEffectConfig c;
        for (int i = 0; i < 200 && !stop.load(); ++i) {
            c.emission_rate = static_cast<float>(i % 100);
            s.set_config(c);
        }
    });
    for (int i = 0; i < 50; ++i) s.update(1.0f / 60.0f);
    stop.store(true);
    writer.join();
    EXPECT_TRUE(true); // reached without crashing
}
