#include "ergo/gpu_particle/emitter_descriptor.h"
#include "gtest/gtest.h"

#include <string>

using namespace ergo::gpu_particle;

TEST(EmitterDescriptor, DefaultsValidate) {
    EmitterDescriptor d;
    std::string err;
    EXPECT_TRUE(d.validate(&err)) ;
}

TEST(EmitterDescriptor, RejectsZeroMaxParticles) {
    EmitterDescriptor d;
    d.max_particles = 0;
    std::string err;
    EXPECT_FALSE(d.validate(&err));
    EXPECT_FALSE(err.empty());
}

TEST(EmitterDescriptor, RejectsNegativeDuration) {
    EmitterDescriptor d;
    d.duration = -1.0f;
    std::string err;
    EXPECT_FALSE(d.validate(&err));
}

TEST(EmitterDescriptor, RejectsZeroConstantLifetime) {
    EmitterDescriptor d;
    d.start_lifetime = MinMaxCurve::constant(0.0f);
    std::string err;
    EXPECT_FALSE(d.validate(&err));
}

TEST(EmitterDescriptor, RejectsInvertedBurstCount) {
    EmitterDescriptor d;
    Burst b;
    b.time = 0.5f;
    b.count_min = 10;
    b.count_max = 5;
    d.bursts.push_back(b);
    std::string err;
    EXPECT_FALSE(d.validate(&err));
}

TEST(EmitterDescriptor, RejectsOutOfRangeConeAngle) {
    EmitterDescriptor d;
    d.shape = EmitterShape::Cone;
    d.cone_angle_deg = 120.0f;
    std::string err;
    EXPECT_FALSE(d.validate(&err));
}

TEST(EmitterDescriptor, AcceptsInfiniteDurationWithLoop) {
    EmitterDescriptor d;
    d.duration = 0.0f;   // infinite
    d.loop     = true;
    std::string err;
    EXPECT_TRUE(d.validate(&err)) ;
}
