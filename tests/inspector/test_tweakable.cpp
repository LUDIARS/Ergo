#include "ergo/inspector/types.h"
#include "ergo/inspector/tweakable.h"
#include "gtest/gtest.h"

using namespace ergo::inspector;

TEST(Value, FactoriesAndKinds) {
    EXPECT_EQ(Value::of_bool(true).kind, VarKind::Bool);
    EXPECT_EQ(Value::of_int32(42).kind, VarKind::Int32);
    EXPECT_EQ(Value::of_int64(1LL << 40).kind, VarKind::Int64);
    EXPECT_EQ(Value::of_float(1.5f).kind, VarKind::Float);
    EXPECT_EQ(Value::of_double(2.5).kind, VarKind::Double);
    EXPECT_EQ(Value::of_string("hi").kind, VarKind::String);
    EXPECT_EQ(Value::of_color(1, 0, 0).kind, VarKind::Color);
    EXPECT_EQ(Value::of_vec3(1, 2, 3).kind, VarKind::Vec3);
}

TEST(Value, Equality) {
    EXPECT_TRUE(Value::of_bool(true).equals(Value::of_bool(true)));
    EXPECT_FALSE(Value::of_bool(true).equals(Value::of_bool(false)));
    EXPECT_FALSE(Value::of_int32(1).equals(Value::of_int64(1)));   // different kinds
    EXPECT_TRUE(Value::of_double(3.14).equals(Value::of_double(3.14)));
    EXPECT_TRUE(Value::of_string("x").equals(Value::of_string("x")));
    EXPECT_FALSE(Value::of_string("x").equals(Value::of_string("y")));
    EXPECT_TRUE(Value::of_vec3(1, 2, 3).equals(Value::of_vec3(1, 2, 3)));
    EXPECT_FALSE(Value::of_vec3(1, 2, 3).equals(Value::of_vec3(1, 2, 4)));
}

TEST(ClampToMeta, NumericClamp) {
    VarMeta m; m.min = 0.0; m.max = 10.0;
    EXPECT_EQ(clamp_to_meta(Value::of_double(5.0), m).d, 5.0);
    EXPECT_EQ(clamp_to_meta(Value::of_double(-3.0), m).d, 0.0);
    EXPECT_EQ(clamp_to_meta(Value::of_double(99.0), m).d, 10.0);

    EXPECT_EQ(clamp_to_meta(Value::of_int32(20), m).i, 10);
    EXPECT_EQ(clamp_to_meta(Value::of_int64(-5), m).i, 0);
}

TEST(ClampToMeta, NoRangeIsPassThrough) {
    VarMeta m; // min == max == 0 → no clamping
    EXPECT_EQ(clamp_to_meta(Value::of_double(99.0), m).d, 99.0);
    EXPECT_EQ(clamp_to_meta(Value::of_int32(-77), m).i, -77);
}

TEST(ClampToMeta, NonNumericIsUntouched) {
    VarMeta m; m.min = 0; m.max = 10;
    auto s = clamp_to_meta(Value::of_string("hello"), m);
    EXPECT_EQ(s.s, "hello");
    auto c = clamp_to_meta(Value::of_color(2, -1, 5, 1), m);
    // Color is intentionally not clamped by clamp_to_meta — the host UI handles range.
    EXPECT_EQ(c.v[0], 2.0f);
    EXPECT_EQ(c.v[1], -1.0f);
}

TEST(ToString, KindNames) {
    EXPECT_EQ(std::string(to_string(VarKind::Bool)),   "bool");
    EXPECT_EQ(std::string(to_string(VarKind::Double)), "double");
    EXPECT_EQ(std::string(to_string(VarKind::Color)),  "color");
}
