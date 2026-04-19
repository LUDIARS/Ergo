#include "ergo/inspector/inspector.h"
#include "gtest/gtest.h"

#include <atomic>

using namespace ergo::inspector;

namespace {
// The Inspector is a singleton — reset registry between tests by unregistering
// everything we added. We can't truly "reset" but we use unique names per test.
}

TEST(Inspector, RegisterAndReadDouble) {
    auto& I = Inspector::instance();
    double v = 60.0;
    Handle h = I.register_value<double>("test_inspector.bpm_a", &v);
    ASSERT_NE(h, INVALID_HANDLE);

    Value out;
    ASSERT_TRUE(I.read_value(h, out));
    EXPECT_EQ(out.kind, VarKind::Double);
    EXPECT_EQ(out.d, 60.0);

    I.unregister(h);
    EXPECT_FALSE(I.read_value(h, out));
}

TEST(Inspector, EnqueueAndApply) {
    auto& I = Inspector::instance();
    int32_t v = 0;
    Handle h = I.register_value<int32_t>("test_inspector.counter_b", &v);
    ASSERT_NE(h, INVALID_HANDLE);

    I.enqueue_write(h, Value::of_int32(42));
    EXPECT_EQ(v, 0); // not yet applied
    I.apply_pending_writes();
    EXPECT_EQ(v, 42);
    I.unregister(h);
}

TEST(Inspector, ReadOnlyIgnoresWrites) {
    auto& I = Inspector::instance();
    double v = 1.0;
    VarMeta m; m.read_only = true;
    Handle h = I.register_value<double>("test_inspector.ro_c", &v, m);
    I.enqueue_write(h, Value::of_double(99.0));
    I.apply_pending_writes();
    EXPECT_EQ(v, 1.0);
    I.unregister(h);
}

TEST(Inspector, RangeClampOnApply) {
    auto& I = Inspector::instance();
    int32_t v = 0;
    VarMeta m; m.min = 0; m.max = 100;
    Handle h = I.register_value<int32_t>("test_inspector.ranged_d", &v, m);
    I.enqueue_write(h, Value::of_int32(500));
    I.apply_pending_writes();
    EXPECT_EQ(v, 100);
    I.enqueue_write(h, Value::of_int32(-50));
    I.apply_pending_writes();
    EXPECT_EQ(v, 0);
    I.unregister(h);
}

TEST(Inspector, TypeMismatchDropped) {
    auto& I = Inspector::instance();
    double v = 1.0;
    Handle h = I.register_value<double>("test_inspector.dbl_e", &v);
    // Push an int — wrong kind, should be ignored on apply.
    I.enqueue_write(h, Value::of_int32(7));
    I.apply_pending_writes();
    EXPECT_EQ(v, 1.0);
    I.unregister(h);
}

TEST(Inspector, DuplicateNameOverwrites) {
    auto& I = Inspector::instance();
    int32_t a = 1, b = 2;
    Handle ha = I.register_value<int32_t>("test_inspector.dup_f", &a);
    Handle hb = I.register_value<int32_t>("test_inspector.dup_f", &b);
    EXPECT_NE(ha, hb);

    Value out;
    EXPECT_FALSE(I.read_value(ha, out)); // old handle is gone
    EXPECT_TRUE(I.read_value(hb, out));
    EXPECT_EQ(out.i, 2);

    I.unregister(hb);
}

TEST(Inspector, FindByName) {
    auto& I = Inspector::instance();
    bool b = false;
    Handle h = I.register_value<bool>("test_inspector.flag_g", &b);
    EXPECT_EQ(I.find_by_name("test_inspector.flag_g"), h);
    EXPECT_EQ(I.find_by_name("test_inspector.does_not_exist"), INVALID_HANDLE);
    I.unregister(h);
}

TEST(Inspector, AccessorRegistration) {
    auto& I = Inspector::instance();
    std::atomic<double> backing{1.0};
    Handle h = I.register_accessor(
        "test_inspector.accessor_h", VarKind::Double,
        [&]{ return Value::of_double(backing.load()); },
        [&](const Value& v){ backing.store(v.d); });

    Value out;
    ASSERT_TRUE(I.read_value(h, out));
    EXPECT_EQ(out.d, 1.0);

    I.enqueue_write(h, Value::of_double(7.5));
    I.apply_pending_writes();
    EXPECT_EQ(backing.load(), 7.5);

    I.unregister(h);
}

TEST(Inspector, SnapshotIncludesRegisteredVar) {
    auto& I = Inspector::instance();
    int32_t v = 5;
    Handle h = I.register_value<int32_t>("test_inspector.snap_i", &v);

    auto snap = I.snapshot();
    bool found = false;
    for (auto& tw : snap) if (tw.name == "test_inspector.snap_i") { found = true; break; }
    EXPECT_TRUE(found);

    I.unregister(h);
}
