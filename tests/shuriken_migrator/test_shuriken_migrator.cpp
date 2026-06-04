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

const char* kBurstPrefab = R"(%YAML 1.1
%TAG !u! tag:unity3d.com,2011:
--- !u!1 &200000
GameObject:
  m_Name: BurstOnly
  m_Component:
  - component: {fileID: 200001}
--- !u!198 &200001
ParticleSystem:
  m_GameObject: {fileID: 200000}
  lengthInSec: 1
  looping: 0
  maxNumParticles: 100
  InitialModule:
    startLifetime:
      minMaxState: 0
      scalar: 1
      scalarMultiplier: 1
    startSpeed:
      minMaxState: 0
      scalar: 1
      scalarMultiplier: 1
    startSize:
      minMaxState: 0
      scalar: 1
      scalarMultiplier: 1
  EmissionModule:
    enabled: 1
    rateOverTime:
      minMaxState: 0
      scalar: 0
      scalarMultiplier: 1
    m_BurstCount: 1
    m_Bursts:
    - serializedVersion: 2
      time: 0.25
      countCurve:
        serializedVersion: 2
        minMaxState: 2
        scalar: 0
        minScalar: 3
        maxScalar: 7
        scalarMultiplier: 1
      cycleCount: 2
      repeatInterval: 0.5
      probability: 0.75
  ShapeModule:
    enabled: 1
    type: 5
    radius: 1
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

TEST(ShurikenMigrator, ConvertsEmissionBurstsToGpuDescriptor) {
    std::vector<ShurikenSource> systems;
    MigrationReport report;
    ASSERT_TRUE(ParsePrefabYaml(kBurstPrefab, systems, report));
    ASSERT_EQ(systems.size(), 1u);
    ASSERT_EQ(systems[0].emission.bursts.size(), 1u);

    auto desc = ConvertToGpuEmitter(systems[0], report);
    ASSERT_EQ(desc.bursts.size(), 1u);
    EXPECT_FLOAT_EQ(desc.rate_over_time.constant_min, 0.0f);
    EXPECT_FLOAT_EQ(desc.bursts[0].time, 0.25f);
    EXPECT_EQ(desc.bursts[0].count_min, 3u);
    EXPECT_EQ(desc.bursts[0].count_max, 7u);
    EXPECT_EQ(desc.bursts[0].cycles, 2u);
    EXPECT_FLOAT_EQ(desc.bursts[0].interval, 0.5f);
    EXPECT_FLOAT_EQ(desc.bursts[0].probability, 0.75f);

    const std::string json = EmitterDescriptorToJson(desc);
    EXPECT_NE(json.find("\"bursts\""), std::string::npos);
    EXPECT_NE(json.find("\"count_min\":3"), std::string::npos);
    EXPECT_NE(json.find("\"count_max\":7"), std::string::npos);
}

TEST(ShurikenMigrator, NoSystemsWhenNoBlock) {
    std::vector<ShurikenSource> systems;
    MigrationReport report;
    ASSERT_TRUE(ParsePrefabYaml("%YAML 1.1\n--- !u!1 &1\nGameObject:\n  m_Name: x\n", systems, report));
    EXPECT_TRUE(systems.empty());
    EXPECT_EQ(report.extractedSystems, 0);
}
