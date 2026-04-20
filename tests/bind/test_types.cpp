#include "ergo/bind/types.h"
#include "gtest/gtest.h"

using namespace ergo::bind;

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
    EXPECT_FALSE(Value::of_int32(1).equals(Value::of_int64(1)));
    EXPECT_TRUE(Value::of_string("x").equals(Value::of_string("x")));
}

TEST(ClampToMeta, Numeric) {
    VarMeta m; m.min = 0.0; m.max = 10.0;
    EXPECT_EQ(clamp_to_meta(Value::of_double(5.0), m).d, 5.0);
    EXPECT_EQ(clamp_to_meta(Value::of_double(-3.0), m).d, 0.0);
    EXPECT_EQ(clamp_to_meta(Value::of_double(99.0), m).d, 10.0);
    EXPECT_EQ(clamp_to_meta(Value::of_int32(20), m).i, 10);
}

TEST(ClampToMeta, NoRangeIsPassThrough) {
    VarMeta m;
    EXPECT_EQ(clamp_to_meta(Value::of_double(99.0), m).d, 99.0);
}

TEST(KindString, RoundTrip) {
    for (auto k : {VarKind::Bool, VarKind::Int32, VarKind::Int64,
                   VarKind::Float, VarKind::Double, VarKind::String,
                   VarKind::Color, VarKind::Vec3}) {
        EXPECT_EQ(kind_from_string(to_string(k)), k);
    }
}
