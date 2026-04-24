#include "ergo/gpu_particle/particle_system.h"
#include "gtest/gtest.h"

using namespace ergo::gpu_particle;

namespace {

// Minimal CPU-fallback exercise: no backend, allow_cpu_fallback = true.
ParticleSystemConfig cpu_config() {
    ParticleSystemConfig c;
    c.spawn_shader_spirv       = nullptr;
    c.spawn_shader_spirv_size  = 0;
    c.update_shader_spirv      = nullptr;
    c.update_shader_spirv_size = 0;
    c.allow_cpu_fallback       = true;
    return c;
}

} // namespace

TEST(ParticleSystem, InitializesInCpuFallback) {
    ParticleSystem sys;
    EXPECT_TRUE(sys.initialize(nullptr, cpu_config()));
    EXPECT_TRUE(sys.is_initialized());
}

TEST(ParticleSystem, FailsWithoutBackendWhenFallbackDisabled) {
    ParticleSystem sys;
    ParticleSystemConfig c = cpu_config();
    c.allow_cpu_fallback = false;
    EXPECT_FALSE(sys.initialize(nullptr, c));
}

TEST(ParticleSystem, CreatesAndDestroysEmitter) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.name = "sparkle";
    d.max_particles = 256;
    d.start_lifetime = MinMaxCurve::constant(1.0f);

    std::string err;
    EmitterHandle h = sys.create_emitter(d, &err);
    ASSERT_NE(h, INVALID_EMITTER) ;
    EXPECT_EQ(sys.get_state(h), EmitterState::Stopped);
    EXPECT_NE(sys.get_descriptor(h), nullptr);

    sys.destroy_emitter(h);
    EXPECT_EQ(sys.get_descriptor(h), nullptr);
}

TEST(ParticleSystem, PlayStopStateMachine) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());
    EmitterDescriptor d;
    d.max_particles = 64;
    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);

    EXPECT_EQ(sys.get_state(h), EmitterState::Stopped);
    sys.play(h);
    EXPECT_EQ(sys.get_state(h), EmitterState::Playing);
    sys.pause(h);
    EXPECT_EQ(sys.get_state(h), EmitterState::Paused);
    sys.stop(h);
    EXPECT_EQ(sys.get_state(h), EmitterState::Stopped);
}

TEST(ParticleSystem, CpuFallbackUpdatesAliveCount) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles   = 256;
    d.duration        = 0.0f;            // infinite
    d.loop            = true;
    d.start_lifetime  = MinMaxCurve::constant(1.0f);
    d.start_speed     = MinMaxCurve::constant(0.0f);
    d.rate_over_time  = MinMaxCurve::constant(100.0f);  // 100 per second

    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);
    sys.play(h);

    // Step ~0.1s in 5 steps. Expect alive count > 0.
    for (int i = 0; i < 5; ++i) sys.update(0.02f);
    EXPECT_GT(sys.get_live_particle_count(h), 0u);
    EXPECT_LE(sys.get_live_particle_count(h), d.max_particles);
}

// ---------------------------------------------------------------------------
// Shape sampling — verifies the CPU fallback honours EmitterDescriptor.shape
// across every shape kind the GLSL kernel also handles. The public API
// doesn't expose raw positions, so these are smoke tests: they check that
// each shape spawns particles without crashing and the count stays within
// `max_particles`.
// ---------------------------------------------------------------------------

TEST(ParticleSystem, SpawnsWithBoxShape) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles  = 64;
    d.duration       = 0.0f;
    d.loop           = true;
    d.start_lifetime = MinMaxCurve::constant(1.0f);
    d.start_speed    = MinMaxCurve::constant(1.0f);
    d.rate_over_time = MinMaxCurve::constant(200.0f);
    d.shape          = EmitterShape::Box;
    d.box_extents    = {2.0f, 2.0f, 2.0f};
    d.random_seed    = 0xBEEF0001u;

    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);
    sys.play(h);
    for (int i = 0; i < 10; ++i) sys.update(0.02f);
    EXPECT_GT(sys.get_live_particle_count(h), 0u);
}

TEST(ParticleSystem, SpawnsWithSphereShape) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles  = 64;
    d.duration       = 0.0f;
    d.loop           = true;
    d.start_lifetime = MinMaxCurve::constant(1.0f);
    d.start_speed    = MinMaxCurve::constant(1.0f);
    d.rate_over_time = MinMaxCurve::constant(200.0f);
    d.shape          = EmitterShape::Sphere;
    d.sphere_radius  = 1.5f;
    d.random_seed    = 0xBEEF0002u;

    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);
    sys.play(h);
    for (int i = 0; i < 10; ++i) sys.update(0.02f);
    EXPECT_GT(sys.get_live_particle_count(h), 0u);
}

TEST(ParticleSystem, SpawnsWithHemisphereShape) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles  = 64;
    d.duration       = 0.0f;
    d.loop           = true;
    d.start_lifetime = MinMaxCurve::constant(1.0f);
    d.start_speed    = MinMaxCurve::constant(0.5f);
    d.rate_over_time = MinMaxCurve::constant(200.0f);
    d.shape          = EmitterShape::Hemisphere;
    d.sphere_radius  = 1.0f;
    d.random_seed    = 0xBEEF0003u;

    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);
    sys.play(h);
    for (int i = 0; i < 10; ++i) sys.update(0.02f);
    EXPECT_GT(sys.get_live_particle_count(h), 0u);
}

TEST(ParticleSystem, SpawnsWithCircleShape) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles  = 64;
    d.duration       = 0.0f;
    d.loop           = true;
    d.start_lifetime = MinMaxCurve::constant(1.0f);
    d.start_speed    = MinMaxCurve::constant(0.0f);   // stay at spawn
    d.rate_over_time = MinMaxCurve::constant(200.0f);
    d.shape          = EmitterShape::Circle;
    d.sphere_radius  = 1.0f;
    d.random_seed    = 0xBEEF0004u;

    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);
    sys.play(h);
    for (int i = 0; i < 10; ++i) sys.update(0.02f);
    EXPECT_GT(sys.get_live_particle_count(h), 0u);
}

// ---------------------------------------------------------------------------
// Bursts: verify that cycles/interval repeats fire within the expected
// window, and that a probability gate can skip fires.
// ---------------------------------------------------------------------------

TEST(ParticleSystem, BurstCyclesFireEachInterval) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles  = 128;
    d.duration       = 0.0f;
    d.loop           = true;
    d.start_lifetime = MinMaxCurve::constant(10.0f);   // keep alive through test
    d.start_speed    = MinMaxCurve::constant(0.0f);
    d.rate_over_time = MinMaxCurve::constant(0.0f);    // bursts only

    Burst b;
    b.time        = 0.05f;
    b.count_min   = 5;
    b.count_max   = 5;
    b.cycles      = 3;
    b.interval    = 0.1f;
    b.probability = 1.0f;
    d.bursts.push_back(b);

    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);
    sys.play(h);

    // First burst at t=0.05. After 0.06s only one fire has happened.
    for (int i = 0; i < 3; ++i) sys.update(0.02f);
    EXPECT_EQ(sys.get_live_particle_count(h), 5u);

    // Advance past t=0.15: 2 fires total.
    for (int i = 0; i < 5; ++i) sys.update(0.02f);
    EXPECT_EQ(sys.get_live_particle_count(h), 10u);

    // Advance past t=0.25: 3 fires total (cycles exhausted).
    for (int i = 0; i < 5; ++i) sys.update(0.02f);
    EXPECT_EQ(sys.get_live_particle_count(h), 15u);

    // Further advance should not add more.
    for (int i = 0; i < 10; ++i) sys.update(0.02f);
    EXPECT_EQ(sys.get_live_particle_count(h), 15u);
}

TEST(ParticleSystem, BurstProbabilityZeroSkipsAllFires) {
    ParticleSystem sys;
    sys.initialize(nullptr, cpu_config());

    EmitterDescriptor d;
    d.max_particles  = 64;
    d.duration       = 0.0f;
    d.loop           = true;
    d.start_lifetime = MinMaxCurve::constant(10.0f);
    d.start_speed    = MinMaxCurve::constant(0.0f);
    d.rate_over_time = MinMaxCurve::constant(0.0f);

    Burst b;
    b.time        = 0.02f;
    b.count_min   = 10;
    b.count_max   = 10;
    b.cycles      = 5;
    b.interval    = 0.05f;
    b.probability = 0.0f;   // never fire
    d.bursts.push_back(b);

    EmitterHandle h = sys.create_emitter(d);
    ASSERT_NE(h, INVALID_EMITTER);
    sys.play(h);
    for (int i = 0; i < 20; ++i) sys.update(0.02f);
    EXPECT_EQ(sys.get_live_particle_count(h), 0u);
}
