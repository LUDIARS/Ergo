#include "ergo/particle/effect_config.h"
#include "gtest/gtest.h"

using namespace ergo::particle;

TEST(EffectConfig, DefaultsAreSane) {
    ParticleEffectConfig c;
    EXPECT_EQ(c.version, SCHEMA_VERSION);
    EXPECT_GT(c.emission_rate, 0.0f);
    EXPECT_GT(c.emission_max_alive, 0);
    EXPECT_LT(c.init_lifetime_min, c.init_lifetime_max);
    EXPECT_LT(c.init_speed_min, c.init_speed_max);
    EXPECT_EQ(c.render_blend, BlendMode::Additive);
}

TEST(EffectConfig, ParsesFullDocument) {
    const std::string js = R"({
        "version": 1,
        "name": "explosion",
        "emission": {"rate": 0, "maxAlive": 300},
        "initial": {
            "positionRadius": 2,
            "velocityAngleDeg": 0,
            "velocityAngleSpreadDeg": 360,
            "speedMin": 120,
            "speedMax": 300,
            "lifetimeMin": 0.4,
            "lifetimeMax": 0.9,
            "size": 10,
            "color": [1, 0.7, 0.2, 1]
        },
        "overLife": {
            "sizeStart": 1, "sizeEnd": 0,
            "colorStart": [1, 0.85, 0.3, 1],
            "colorEnd":   [0.4, 0.05, 0, 0],
            "velocityDamping": 0.2
        },
        "forces": {"gravity": [0, 80]},
        "render": {"blend": "alpha", "shape": "square"}
    })";

    ParticleEffectConfig c;
    ASSERT_TRUE(parse_config_json(js, c));
    EXPECT_EQ(c.name, "explosion");
    EXPECT_EQ(c.emission_rate, 0.0f);
    EXPECT_EQ(c.emission_max_alive, 300);
    EXPECT_EQ(c.init_velocity_spread_deg, 360.0f);
    EXPECT_EQ(c.init_color[0], 1.0f);
    EXPECT_EQ(c.init_color[2], 0.2f);
    EXPECT_EQ(c.life_color_end[3], 0.0f);
    EXPECT_EQ(c.gravity[1], 80.0f);
    EXPECT_EQ(c.render_blend, BlendMode::Alpha);
    EXPECT_EQ(c.render_shape, ShapeMode::Square);
}

TEST(EffectConfig, DeepMergeKeepsExisting) {
    ParticleEffectConfig c;
    c.name = "kept";
    c.gravity = {99.0f, 99.0f};
    const std::string js = R"({"emission":{"rate":7}})";
    ASSERT_TRUE(parse_config_json(js, c));
    EXPECT_EQ(c.name, "kept");                 // untouched
    EXPECT_EQ(c.emission_rate, 7.0f);          // updated
    EXPECT_EQ(c.gravity[0], 99.0f);            // untouched
    EXPECT_EQ(c.gravity[1], 99.0f);            // untouched
}

TEST(EffectConfig, IgnoresUnknownFields) {
    ParticleEffectConfig c;
    const std::string js = R"({"unknown":42,"emission":{"rate":50,"junk":[1,2,3]}})";
    ASSERT_TRUE(parse_config_json(js, c));
    EXPECT_EQ(c.emission_rate, 50.0f);
}

TEST(EffectConfig, RejectsNonObjectRoot) {
    ParticleEffectConfig c;
    EXPECT_FALSE(parse_config_json("[]", c));
    EXPECT_FALSE(parse_config_json("\"hi\"", c));
    EXPECT_FALSE(parse_config_json("", c));
}
