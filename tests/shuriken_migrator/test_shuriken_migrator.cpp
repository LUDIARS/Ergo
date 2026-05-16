#include "ergo/shuriken_migrator/migrator.h"

#include "gtest/gtest.h"

#include <string>

using namespace ergo::shuriken_migrator;

namespace {

// 最小 Unity prefab YAML 風 (ParticleSystem 1 個 + 直前の GameObject)
const char* kMinimalPrefab = R"(%YAML 1.1
%TAG !u! tag:unity3d.com,2011:
--- !u!1 &100000
GameObject:
  m_Name: HitEffect
  m_Component:
  - component: {fileID: 100001}
--- !u!198 &100001
ParticleSystem:
  m_GameObject: {fileID: 100000}
  lengthInSec: 2.5
  looping: 1
  simulationSpace: 0
  maxNumParticles: 250
  InitialModule:
    startLifetime:
      minMaxState: 0
      scalar: 1.4
      scalarMultiplier: 1
    startSpeed:
      minMaxState: 2
      minScalar: 80
      maxScalar: 180
      scalarMultiplier: 1
    startSize:
      minMaxState: 0
      scalar: 8
      scalarMultiplier: 1
    startColor:
      maxColor:
        r: 1
        g: 0.75
        b: 0.25
        a: 1
    gravityModifier:
      minMaxState: 0
      scalar: 0
      scalarMultiplier: 1
  EmissionModule:
    enabled: 1
    rateOverTime:
      minMaxState: 0
      scalar: 60
      scalarMultiplier: 1
  ShapeModule:
    enabled: 1
    type: 5
    radius: 0.4
    angle: 25
)";

}  // namespace

TEST(ShurikenMigrator, ParseMinimalPrefab) {
    std::vector<ShurikenSource> systems;
    MigrationReport report;
    ASSERT_TRUE(ParsePrefabYaml(kMinimalPrefab, systems, report));
    ASSERT_EQ(systems.size(), 1u);
    EXPECT_EQ(systems[0].name, "HitEffect");
}

TEST(ShurikenMigrator, ConvertCoreFields) {
    std::vector<ShurikenSource> systems;
    MigrationReport report;
    ASSERT_TRUE(ParsePrefabYaml(kMinimalPrefab, systems, report));
    auto cfg = ConvertToErgo(systems[0], report);

    EXPECT_EQ(cfg.name, "HitEffect");
    EXPECT_FLOAT_EQ(cfg.init_lifetime_min, 1.4f);
    EXPECT_FLOAT_EQ(cfg.init_lifetime_max, 1.4f);
    EXPECT_FLOAT_EQ(cfg.init_speed_min, 80.0f);
    EXPECT_FLOAT_EQ(cfg.init_speed_max, 180.0f);
    EXPECT_FLOAT_EQ(cfg.init_size, 8.0f);
    EXPECT_FLOAT_EQ(cfg.emission_rate, 60.0f);
    EXPECT_FLOAT_EQ(cfg.init_position_radius, 0.4f);
    // Cone なので spread = angle
    EXPECT_FLOAT_EQ(cfg.init_velocity_spread_deg, 25.0f);

    EXPECT_FLOAT_EQ(cfg.init_color[0], 1.0f);
    EXPECT_FLOAT_EQ(cfg.init_color[1], 0.75f);
    EXPECT_FLOAT_EQ(cfg.init_color[2], 0.25f);
    EXPECT_FLOAT_EQ(cfg.init_color[3], 1.0f);
}

TEST(ShurikenMigrator, MinMaxStateTwoConstants) {
    MinMaxCurve c;
    c.state = MinMaxState::TwoConstants;
    c.minScalar = 10.0f;
    c.maxScalar = 50.0f;
    c.scalarMultiplier = 2.0f;
    float mn, mx;
    c.Range(mn, mx);
    EXPECT_FLOAT_EQ(mn, 20.0f);
    EXPECT_FLOAT_EQ(mx, 100.0f);
}

TEST(ShurikenMigrator, NoSystemsWhenNoBlock) {
    std::vector<ShurikenSource> systems;
    MigrationReport report;
    ASSERT_TRUE(ParsePrefabYaml("%YAML 1.1\n--- !u!1 &1\nGameObject:\n  m_Name: x\n", systems, report));
    EXPECT_TRUE(systems.empty());
    EXPECT_EQ(report.extractedSystems, 0);
}
