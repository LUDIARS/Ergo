#include "ergo/health/health.h"

#include "gtest/gtest.h"

using ergo::health::Config;
using ergo::health::Health;

TEST(Health, StartsAtMaxAndAlive) {
    Health h(Config{100});
    EXPECT_EQ(h.hp(), 100);
    EXPECT_EQ(h.max_hp(), 100);
    EXPECT_FALSE(h.is_dead());
}

TEST(Health, DamageReducesHpAndFloorsAtZero) {
    Health h(Config{100});
    h.apply_damage(30);
    EXPECT_EQ(h.hp(), 70);
    h.apply_damage(200);
    EXPECT_EQ(h.hp(), 0);
    EXPECT_TRUE(h.is_dead());
}

TEST(Health, HealCapsAtMax) {
    Health h(Config{100});
    h.apply_damage(50);
    h.heal(70);
    EXPECT_EQ(h.hp(), 100);
}

TEST(Health, HealOnDeadDoesNothing) {
    Health h(Config{100});
    h.apply_damage(200);
    EXPECT_TRUE(h.is_dead());
    h.heal(50);
    EXPECT_EQ(h.hp(), 0);
}

TEST(Health, OnDamageAndOnDeathFireOnceEach) {
    Health h(Config{100});
    int dmg_calls = 0;
    int last_dmg = 0;
    int last_hp_after = -1;
    h.set_on_damage([&](int amt, int after) {
        ++dmg_calls;
        last_dmg = amt;
        last_hp_after = after;
    });
    int death_calls = 0;
    h.set_on_death([&] { ++death_calls; });

    h.apply_damage(40);
    EXPECT_EQ(dmg_calls, 1);
    EXPECT_EQ(last_dmg, 40);
    EXPECT_EQ(last_hp_after, 60);
    EXPECT_EQ(death_calls, 0);

    h.apply_damage(60);
    EXPECT_EQ(dmg_calls, 2);
    EXPECT_EQ(death_calls, 1);

    // Damaging a corpse is ignored.
    h.apply_damage(10);
    EXPECT_EQ(dmg_calls, 2);
    EXPECT_EQ(death_calls, 1);
}

TEST(Health, RegenAccumulatesFractional) {
    Config cfg{100};
    cfg.regen_per_second = 5.0f;
    Health h(cfg);
    h.apply_damage(50);
    // 0.1s * 5/s = 0.5 -> no whole point yet.
    h.tick(0.1f);
    EXPECT_EQ(h.hp(), 50);
    // Accumulator now 0.5 + 0.5 = 1.0 -> +1 hp.
    h.tick(0.1f);
    EXPECT_EQ(h.hp(), 51);
}

TEST(Health, RegenRespectsMax) {
    Config cfg{100};
    cfg.regen_per_second = 1000.0f;
    Health h(cfg);
    h.apply_damage(10);
    h.tick(1.0f);
    EXPECT_EQ(h.hp(), 100);
}

TEST(Health, ReviveResetsHp) {
    Health h(Config{100});
    h.apply_damage(200);
    EXPECT_TRUE(h.is_dead());
    h.revive();
    EXPECT_EQ(h.hp(), 100);
    EXPECT_FALSE(h.is_dead());
}

TEST(Health, RatioReportsCorrectly) {
    Health h(Config{200});
    h.apply_damage(50);
    EXPECT_FLOAT_EQ(h.ratio(), 150.0f / 200.0f);
}
