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
