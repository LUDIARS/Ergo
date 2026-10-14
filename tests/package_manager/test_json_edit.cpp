// 書式を保つ JSON の書き換え (spec/feature/package-manager.md §2.2: キー順・改行・字下げを変えず、 該当の値だけ)。

#include "ergo/package_manager/json_edit.h"

#include "gtest/gtest.h"

#include <string>

using namespace ergo::package_manager;

namespace {

const std::string kDoc =
    "{\n"
    "  \"schema\": 1,\n"
    "  \"host\": {\n"
    "    \"code\": \"Example\",\n"
    "    \"ergo\": { \"repository\": \"https://example.invalid/Ergo.git\", \"rev\": \"aaa\" },\n"
    "    \"use\": [\"pictor\", \"tela\"],\n"
    "    \"optional\": [],\n"
    "    \"features\": {}\n"
    "  },\n"
    "  \"packages\": {}\n"
    "}\n";

} // namespace

TEST(PackageManagerJsonEdit, ReplaceChangesOnlyTheValue) {
    std::string t = kDoc;
    ASSERT_TRUE(json_replace(t, {"host", "ergo", "rev"}, json_quote("bbb")));
    std::string expected = kDoc;
    expected.replace(expected.find("\"aaa\""), 5, "\"bbb\"");
    EXPECT_EQ(t, expected);
}

TEST(PackageManagerJsonEdit, SetMemberAppendsInTheSiblingStyle) {
    std::string t = kDoc;
    ASSERT_TRUE(json_set_member(t, {"host", "ergo"}, "expect_tag", json_quote("v1.0.0")));
    EXPECT_NE(t.find("\"rev\": \"aaa\", \"expect_tag\": \"v1.0.0\" }"), std::string::npos);
    ASSERT_TRUE(json_set_member(t, {"host"}, "extra", "true"));
    EXPECT_NE(t.find("\"features\": {},\n    \"extra\": true\n  },"), std::string::npos);
}

TEST(PackageManagerJsonEdit, ArraysKeepTheirLayout) {
    std::string t = kDoc;
    ASSERT_TRUE(json_append_string(t, {"host", "optional"}, "pictor"));
    EXPECT_NE(t.find("\"optional\": [\"pictor\"],"), std::string::npos);
    ASSERT_TRUE(json_append_string(t, {"host", "optional"}, "tela"));
    EXPECT_NE(t.find("\"optional\": [\"pictor\", \"tela\"],"), std::string::npos);
    ASSERT_TRUE(json_append_string(t, {"host", "optional"}, "tela"));  // already there
    EXPECT_NE(t.find("\"optional\": [\"pictor\", \"tela\"],"), std::string::npos);

    ASSERT_TRUE(json_remove_string(t, {"host", "use"}, "pictor"));
    EXPECT_NE(t.find("\"use\": [\"tela\"],"), std::string::npos);
    ASSERT_TRUE(json_remove_string(t, {"host", "use"}, "tela"));
    EXPECT_NE(t.find("\"use\": [],"), std::string::npos);
}

TEST(PackageManagerJsonEdit, AddThenRemoveObjectMemberRoundTrips) {
    std::string t = kDoc;
    ASSERT_TRUE(json_add_object_member(t, {"packages"}, "privlib",
                                       {{"kind", json_quote("link")}, {"rev", json_quote("ccc")}}));
    EXPECT_NE(t.find("  \"packages\": {\n    \"privlib\": {\n      \"kind\": \"link\",\n      \"rev\": \"ccc\"\n"
                     "    }\n  }\n}"),
              std::string::npos);
    EXPECT_FALSE(json_add_object_member(t, {"packages"}, "privlib", {}));  // already there
    ASSERT_TRUE(json_add_object_member(t, {"packages"}, "other", {{"kind", json_quote("tool")}}));
    EXPECT_NE(t.find("    },\n    \"other\": {\n      \"kind\": \"tool\"\n    }\n  }"), std::string::npos);

    ASSERT_TRUE(json_remove_member(t, {"packages"}, "privlib"));
    ASSERT_TRUE(json_remove_member(t, {"packages"}, "other"));
    EXPECT_NE(t.find("\"packages\": {}"), std::string::npos);
}

TEST(PackageManagerJsonEdit, RemoveFirstMiddleLastMembers) {
    std::string t = "{\n  \"a\": 1,\n  \"b\": 2,\n  \"c\": 3\n}\n";
    std::string u = t;
    ASSERT_TRUE(json_remove_member(u, {}, "a"));
    EXPECT_EQ(u, "{\n  \"b\": 2,\n  \"c\": 3\n}\n");
    u = t;
    ASSERT_TRUE(json_remove_member(u, {}, "b"));
    EXPECT_EQ(u, "{\n  \"a\": 1,\n  \"c\": 3\n}\n");
    u = t;
    ASSERT_TRUE(json_remove_member(u, {}, "c"));
    EXPECT_EQ(u, "{\n  \"a\": 1,\n  \"b\": 2\n}\n");
    ASSERT_TRUE(json_remove_member(u, {}, "missing"));
    EXPECT_EQ(u, "{\n  \"a\": 1,\n  \"b\": 2\n}\n");
}

TEST(PackageManagerJsonEdit, KeepsCrlfAndNestedStrings) {
    std::string t = "{\r\n  \"s\": \"a } ] , \\\" b\",\r\n  \"o\": {}\r\n}\r\n";
    ASSERT_TRUE(json_add_object_member(t, {"o"}, "k", {{"v", "1"}}));
    EXPECT_EQ(t, "{\r\n  \"s\": \"a } ] , \\\" b\",\r\n  \"o\": {\r\n    \"k\": {\r\n      \"v\": 1\r\n    }\r\n  }\r\n}\r\n");
    ASSERT_TRUE(json_set_member(t, {}, "s", json_quote("x")));
    EXPECT_NE(t.find("\"s\": \"x\",\r\n"), std::string::npos);
}

TEST(PackageManagerJsonEdit, MissingPathsFailWithoutChanges) {
    std::string t = kDoc;
    EXPECT_FALSE(json_replace(t, {"host", "nope"}, "1"));
    EXPECT_FALSE(json_set_member(t, {"nope"}, "k", "1"));
    EXPECT_FALSE(json_append_string(t, {"host", "features"}, "x"));  // not an array
    EXPECT_EQ(t, kDoc);
    EXPECT_EQ(json_quote("a\"b\\c\n"), "\"a\\\"b\\\\c\\n\"");
}
