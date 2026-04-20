#include "ergo/bind/json_min.h"
#include "gtest/gtest.h"

using namespace ergo::bind::jsonm;

TEST(JsonMin, ParsePrimitives) {
    JsonValue v;
    ASSERT_TRUE(parse("null", v));    EXPECT_TRUE(v.is_null());
    ASSERT_TRUE(parse("true", v));    EXPECT_TRUE(v.is_bool());   EXPECT_TRUE(v.b);
    ASSERT_TRUE(parse("42", v));      EXPECT_TRUE(v.is_number()); EXPECT_EQ(v.n, 42.0);
    ASSERT_TRUE(parse("\"hi\"", v));  EXPECT_TRUE(v.is_string()); EXPECT_EQ(v.s, "hi");
}

TEST(JsonMin, RoundTripObject) {
    auto o = JsonValue::make_object();
    o.set("op",   JsonValue::make_string("set"));
    o.set("name", JsonValue::make_string("player.bpm"));
    o.set("value", JsonValue::make_number(120.0));
    std::string s = serialize(o);
    JsonValue back;
    ASSERT_TRUE(parse(s, back));
    EXPECT_EQ(back.find("op")->s, "set");
    EXPECT_EQ(back.find("value")->n, 120.0);
}

TEST(JsonMin, ArrayOfNumbers) {
    auto a = JsonValue::make_array();
    a.push(JsonValue::make_number(1));
    a.push(JsonValue::make_number(2));
    a.push(JsonValue::make_number(3));
    std::string s = serialize(a);
    JsonValue back;
    ASSERT_TRUE(parse(s, back));
    ASSERT_TRUE(back.is_array());
    EXPECT_EQ(back.a->size(), 3u);
    EXPECT_EQ((*back.a)[2].n, 3.0);
}

TEST(JsonMin, RejectsGarbage) {
    JsonValue v;
    EXPECT_FALSE(parse("{", v));
    EXPECT_FALSE(parse("nope", v));
}
