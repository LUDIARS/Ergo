#include "ergo/inspector/json_min.h"
#include "gtest/gtest.h"

using namespace ergo::inspector::jsonm;

TEST(JsonMin, ParsePrimitives) {
    JsonValue v;
    ASSERT_TRUE(parse("null", v));    EXPECT_TRUE(v.is_null());
    ASSERT_TRUE(parse("true", v));    EXPECT_TRUE(v.is_bool());   EXPECT_TRUE(v.b);
    ASSERT_TRUE(parse("false", v));   EXPECT_TRUE(v.is_bool());   EXPECT_FALSE(v.b);
    ASSERT_TRUE(parse("42", v));      EXPECT_TRUE(v.is_number()); EXPECT_EQ(v.n, 42.0);
    ASSERT_TRUE(parse("-1.5e2", v));  EXPECT_TRUE(v.is_number()); EXPECT_EQ(v.n, -150.0);
    ASSERT_TRUE(parse("\"hi\"", v));  EXPECT_TRUE(v.is_string()); EXPECT_EQ(v.s, "hi");
}

TEST(JsonMin, ParseObjectAndArray) {
    JsonValue v;
    const std::string src = "{\"a\":1,\"b\":[true,\"x\"],\"c\":null}";
    ASSERT_TRUE(parse(src, v));
    ASSERT_TRUE(v.is_object());
    auto* a = v.find("a"); ASSERT_TRUE(a && a->is_number()); EXPECT_EQ(a->n, 1.0);
    auto* b = v.find("b"); ASSERT_TRUE(b && b->is_array());
    ASSERT_EQ(b->a->size(), 2u);
    EXPECT_TRUE((*b->a)[0].is_bool() && (*b->a)[0].b);
    EXPECT_TRUE((*b->a)[1].is_string() && (*b->a)[1].s == "x");
    auto* c = v.find("c"); ASSERT_TRUE(c && c->is_null());
}

TEST(JsonMin, ParseEscapes) {
    JsonValue v;
    const std::string src = "\"a\\nb\\t\\\"c\"";
    ASSERT_TRUE(parse(src, v));
    EXPECT_EQ(v.s, "a\nb\t\"c");
}

TEST(JsonMin, ParseRejectsGarbage) {
    JsonValue v;
    EXPECT_FALSE(parse("{", v));
    EXPECT_FALSE(parse("[1,", v));
    EXPECT_FALSE(parse("\"unterminated", v));
    EXPECT_FALSE(parse("nope", v));
}

TEST(JsonMin, SerializeRoundTripsPrimitives) {
    EXPECT_EQ(serialize(JsonValue::make_null()),         "null");
    EXPECT_EQ(serialize(JsonValue::make_bool(true)),     "true");
    EXPECT_EQ(serialize(JsonValue::make_string("a\"b")), "\"a\\\"b\"");
}

TEST(JsonMin, SerializeObjectIsParsable) {
    auto root = JsonValue::make_object();
    root.set("op",    JsonValue::make_string("set"));
    root.set("name",  JsonValue::make_string("player.bpm"));
    root.set("value", JsonValue::make_number(120.0));

    std::string s = serialize(root);
    JsonValue back;
    ASSERT_TRUE(parse(s, back));
    EXPECT_EQ(back.find("op")->s, "set");
    EXPECT_EQ(back.find("name")->s, "player.bpm");
    EXPECT_EQ(back.find("value")->n, 120.0);
}

TEST(JsonMin, SerializeArrayOfObjects) {
    auto a = JsonValue::make_array();
    auto e1 = JsonValue::make_object();
    e1.set("k", JsonValue::make_number(1));
    a.push(std::move(e1));
    auto e2 = JsonValue::make_object();
    e2.set("k", JsonValue::make_number(2));
    a.push(std::move(e2));

    std::string s = serialize(a);
    JsonValue back;
    ASSERT_TRUE(parse(s, back));
    ASSERT_TRUE(back.is_array());
    EXPECT_EQ(back.a->size(), 2u);
    EXPECT_EQ((*back.a)[0].find("k")->n, 1.0);
    EXPECT_EQ((*back.a)[1].find("k")->n, 2.0);
}
