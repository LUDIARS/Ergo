#include "ergo/common/json_min.h"
#include "gtest/gtest.h"

using namespace ergo::common::jsonm;

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

// --- ParseLimits boundaries (lorica::input_budget per parse) ---------------

namespace {
std::string nested_arrays(std::size_t n) { return std::string(n, '[') + std::string(n, ']'); }
std::string zeros_array(std::size_t elements) {
    std::string s = "[";
    for (std::size_t i = 0; i < elements; ++i) s += i ? ",0" : "0";
    return s + "]";
}
} // namespace

TEST(JsonMinLimits, DepthExactlyAtLimitAndOneOver) {
    JsonValue v;
    // The innermost of 65 arrays sits at depth 64 (the top level is 0).
    EXPECT_TRUE(parse(nested_arrays(65), v));
    EXPECT_FALSE(parse(nested_arrays(66), v));
    ParseLimits shallow;
    shallow.max_depth = 2;
    EXPECT_TRUE(parse("{\"a\":[1]}", v, shallow));
    EXPECT_FALSE(parse("{\"a\":[[1]]}", v, shallow));
}

TEST(JsonMinLimits, ValueCountExactlyAtLimitAndOneOver) {
    JsonValue v;
    ParseLimits limits;
    limits.max_values = 11;  // the array itself + 10 elements
    EXPECT_TRUE(parse(zeros_array(10), v, limits));
    EXPECT_EQ(v.a->size(), 10u);
    EXPECT_FALSE(parse(zeros_array(11), v, limits));
    // Object members count as values too; keys do not.
    limits.max_values = 3;
    EXPECT_TRUE(parse("{\"a\":1,\"b\":\"x\"}", v, limits));
    EXPECT_FALSE(parse("{\"a\":1,\"b\":\"x\",\"c\":null}", v, limits));
}

TEST(JsonMinLimits, InputBytesExactlyAtLimitAndOneOver) {
    JsonValue v;
    ParseLimits limits;
    limits.max_input_bytes = 7;
    EXPECT_TRUE(parse("\"abcde\"", v, limits));
    EXPECT_FALSE(parse("\"abcdef\"", v, limits));
}

TEST(JsonMinLimits, HugeArrayIsRefusedByTheDefaultValueLimit) {
    JsonValue v;
    EXPECT_TRUE(parse(zeros_array((1u << 20) - 1), v));   // + the array = 1,048,576 values
    EXPECT_FALSE(parse(zeros_array(1u << 20), v));
}

TEST(JsonMinLimits, InvalidUnicodeEscapesAreRejected) {
    JsonValue v;
    EXPECT_TRUE(parse("\"\\u0041\"", v));
    EXPECT_EQ(v.s, "A");
    EXPECT_FALSE(parse("\"\\uZZZZ\"", v));
    EXPECT_FALSE(parse("\"\\u12\"", v));
    EXPECT_FALSE(parse("\"\\u", v));
    // A high surrogate followed by a malformed escape is not combined, and the
    // malformed escape itself fails.
    EXPECT_FALSE(parse("\"\\uD83D\\uZZZZ\"", v));
    EXPECT_TRUE(parse("\"\\uD83D\\uDE00\"", v));
    EXPECT_EQ(v.s, "\xF0\x9F\x98\x80");
}

TEST(JsonMinLimits, MalformedDocumentsAreRejected) {
    JsonValue v;
    for (const char* bad : {"", "[", "{", "{\"a\"}", "{\"a\":}", "[1,]", "[1 2]", "tru", "nul", "\"open",
                            "{\"a\":1}x", "[1]]", "-", "{1:2}"}) {
        EXPECT_FALSE(parse(bad, v));
    }
}
