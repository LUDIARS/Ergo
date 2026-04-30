#include "ergo/combo_counter/combo_counter.h"

#include "gtest/gtest.h"

using ergo::combo_counter::ComboCounter;
using ergo::combo_counter::Config;

TEST(ComboCounter, StartsAtZero) {
    ComboCounter cc;
    EXPECT_EQ(cc.count(), 0);
    EXPECT_EQ(cc.peak(), 0);
}

TEST(ComboCounter, HitIncrementsAndUpdatesPeak) {
    ComboCounter cc;
    cc.hit();
    cc.hit();
    cc.hit();
    EXPECT_EQ(cc.count(), 3);
    EXPECT_EQ(cc.peak(), 3);
    cc.break_();
    EXPECT_EQ(cc.count(), 0);
    EXPECT_EQ(cc.peak(), 3);
    cc.hit();
    EXPECT_EQ(cc.peak(), 3);  // not exceeded
}

TEST(ComboCounter, BreakFiresOnlyIfThereWasACombo) {
    ComboCounter cc;
    int break_calls = 0;
    int last_broken = -1;
    cc.set_on_break([&](int n) {
        ++break_calls;
        last_broken = n;
    });
    cc.break_();  // nothing to break
    EXPECT_EQ(break_calls, 0);
    cc.hit();
    cc.hit();
    cc.break_();
    EXPECT_EQ(break_calls, 1);
    EXPECT_EQ(last_broken, 2);
}

TEST(ComboCounter, FullComboFiresAtThresholdOnly) {
    Config cfg;
    cfg.full_combo_threshold = 5;
    ComboCounter cc(cfg);
    int full_calls = 0;
    cc.set_on_full_combo([&](int) { ++full_calls; });
    for (int i = 0; i < 4; ++i) cc.hit();
    EXPECT_EQ(full_calls, 0);
    cc.hit();  // -> 5, fires
    EXPECT_EQ(full_calls, 1);
    cc.hit();  // 6: does NOT fire again
    EXPECT_EQ(full_calls, 1);
}

TEST(ComboCounter, ChangeFiresOnHitAndBreak) {
    ComboCounter cc;
    int changes = 0;
    cc.set_on_change([&](int) { ++changes; });
    cc.hit();  // 1 change
    cc.hit();  // 1 change
    cc.break_();  // 1 change
    cc.break_();  // count was 0, no change
    EXPECT_EQ(changes, 3);
}

TEST(ComboCounter, ResetClearsPeak) {
    ComboCounter cc;
    cc.hit();
    cc.hit();
    EXPECT_EQ(cc.peak(), 2);
    cc.reset();
    EXPECT_EQ(cc.count(), 0);
    EXPECT_EQ(cc.peak(), 0);
}
