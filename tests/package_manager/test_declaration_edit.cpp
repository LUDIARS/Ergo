// W2 の書き換え (spec/feature/package-manager.md §7 W2): update が rev と expect_tag だけを書き他はバイト単位で同じ、
// 持ち主の判定 (共有は Ergo のマニフェスト、 ergo と非公開はホスト)、 remove が needs を壊すと拒否、
// add --kind link で結線ファイルが無ければ誤りでファイル不変。

#include "fixture_util.h"

#include "ergo/package_manager/declaration_edit.h"
#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/host_declaration.h"
#include "ergo/package_manager/package_owner.h"

#include "gtest/gtest.h"

#include <string>

using namespace ergo::package_manager;
using ergo::package_manager::test::fixture_json;

namespace {

/// 見本を LF にそろえて読む (チェックアウトの改行設定に左右されないため。 CRLF の保持は test_json_edit が見る)。
std::string fixture_text(const std::string& name) {
    std::string out;
    for (const char c : ergo::package_manager::test::fixture_text(name)) {
        if (c != '\r') out += c;
    }
    return out;
}

const std::string kNewSha = "3333333333333333333333333333333333333333";

ErgoManifest ergo_manifest() {
    ErgoManifest m;
    std::string error;
    read_ergo_manifest(fixture_json("ergo-manifest.json"), m, &error);
    return m;
}

HostDeclaration host_of(const std::string& text) {
    ergo::common::jsonm::JsonValue doc;
    ergo::common::jsonm::parse(text, doc);
    HostDeclaration h;
    std::vector<Finding> ignored;
    read_host_declaration(doc, h, ignored);
    return h;
}

std::string replaced(std::string s, const std::string& from, const std::string& to) {
    s.replace(s.find(from), from.size(), to);
    return s;
}

AddRequest private_link(const std::string& name) {
    AddRequest r;
    r.name          = name;
    r.repository    = "https://example.invalid/Newlib.git";
    r.rev           = kNewSha;
    r.wiring_exists = true;
    return r;
}

} // namespace

TEST(PackageManagerOwner, SharedIsErgoAndErgoOrPrivateIsHost) {
    const auto m = ergo_manifest();
    const auto h = host_of(fixture_text("host-private.json"));
    EXPECT_TRUE(owner_of("ergo", &h, &m) == PackageOwner::HostErgo);
    EXPECT_TRUE(owner_of("pictor", &h, &m) == PackageOwner::ErgoShared);
    EXPECT_TRUE(owner_of("privlib", &h, &m) == PackageOwner::HostPrivate);
    EXPECT_TRUE(owner_of("nosuch", &h, &m) == PackageOwner::Unknown);
    EXPECT_TRUE(pin_path(PackageOwner::HostErgo, "ergo") == JsonPath({"host", "ergo"}));
    EXPECT_TRUE(pin_path(PackageOwner::ErgoShared, "pictor") == JsonPath({"packages", "pictor"}));

    const auto clash = host_of(replaced(fixture_text("host-private.json"), "\"privlib\": {", "\"tela\": {"));
    EXPECT_TRUE(owner_of("tela", &clash, &m) == PackageOwner::Conflict);
}

TEST(PackageManagerPin, TagWritesOnlyRevAndExpectTag) {
    const std::string before = fixture_text("host-only.json");
    const auto r = plan_pin(before, {"host", "ergo"}, kNewSha, "v1.0.0");
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.changed);
    const std::string expected =
        replaced(replaced(before, "b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kNewSha), "\"v0.9.0\"", "\"v1.0.0\"");
    EXPECT_EQ(r.text, expected);
}

TEST(PackageManagerPin, AddsMissingExpectTagAndDropsItForUntaggedPins) {
    const std::string ergo = fixture_text("ergo-manifest.json");
    const auto tagged = plan_pin(ergo, {"packages", "tela"}, kNewSha, "v1.2.0");
    ASSERT_TRUE(tagged.ok);
    EXPECT_EQ(tagged.text, replaced(ergo, "\"1111111111111111111111111111111111111111\"",
                                    "\"" + kNewSha + "\",\n      \"expect_tag\": \"v1.2.0\""));

    const auto untagged = plan_pin(ergo, {"packages", "pictor"}, kNewSha, "");
    ASSERT_TRUE(untagged.ok);
    EXPECT_EQ(untagged.text, replaced(ergo, "\"625d83d61c90c30ceed3fab2bd2e11a7edf8d39f\",\n      \"expect_tag\": \"v2.2.0\"",
                                      "\"" + kNewSha + "\""));

    EXPECT_FALSE(plan_pin(ergo, {"packages", "pictor"}, "main", "").ok);
    EXPECT_FALSE(plan_pin(ergo, {"packages", "nosuch"}, kNewSha, "").ok);
}

TEST(PackageManagerAdd, SharedGoesToUseOptionalAndFeatures) {
    const auto m = ergo_manifest();
    const std::string before = fixture_text("host-only.json");
    AddRequest r;
    r.name     = "pictor";
    r.optional = true;
    r.features = "+vulkan";
    const auto added = plan_add(before, &m, r);
    ASSERT_TRUE(added.ok);
    EXPECT_EQ(added.text, replaced(replaced(replaced(before, "\"use\": []", "\"use\": [\"pictor\"]"),
                                            "\"optional\": []", "\"optional\": [\"pictor\"]"),
                                   "\"features\": {}", "\"features\": { \"pictor\": \"+vulkan\" }"));

    const auto again = plan_add(added.text, &m, r);
    ASSERT_TRUE(again.ok);
    EXPECT_FALSE(again.changed);

    AddRequest owned = r;
    owned.repository = "https://example.invalid/Fork.git";
    EXPECT_FALSE(plan_add(before, &m, owned).ok);  // the Ergo manifest owns pictor's pin
}

TEST(PackageManagerAdd, PrivateLinkNeedsItsWiringFile) {
    const auto m = ergo_manifest();
    const std::string before = fixture_text("host-only.json");
    AddRequest r = private_link("newlib");
    r.wiring_exists = false;
    const auto refused = plan_add(before, &m, r);
    EXPECT_FALSE(refused.ok);
    EXPECT_NE(refused.error.find("cmake/packages/newlib.cmake"), std::string::npos);

    r.wiring_exists = true;
    const auto added = plan_add(before, &m, r);
    ASSERT_TRUE(added.ok);
    EXPECT_NE(added.text.find("  \"packages\": {\n    \"newlib\": {\n      \"kind\": \"link\",\n"
                              "      \"repository\": \"https://example.invalid/Newlib.git\",\n"
                              "      \"rev\": \"" + kNewSha + "\",\n"
                              "      \"cmake\": { \"wiring\": \"cmake/packages/newlib.cmake\" }\n    }\n  }"),
              std::string::npos);
    EXPECT_NE(added.text.find("\"use\": [\"newlib\"]"), std::string::npos);

    AddRequest tool = private_link("newtool");
    tool.kind          = "tool";
    tool.wiring_exists = false;  // a tool has no wiring
    EXPECT_TRUE(plan_add(before, &m, tool).ok);
}

TEST(PackageManagerAdd, RefusesWhatWouldBreakTheDeclaration) {
    const auto m = ergo_manifest();
    const std::string before = fixture_text("host-only.json");
    AddRequest bare;
    bare.name = "newlib";
    EXPECT_FALSE(plan_add(before, &m, bare).ok);       // private without repository / rev
    EXPECT_FALSE(plan_add(before, nullptr, bare).ok);  // cannot tell shared from private
    AddRequest observed;
    observed.name = "curare";
    EXPECT_FALSE(plan_add(before, &m, observed).ok);   // observed repository, not a package
    AddRequest ergo;
    ergo.name = "ergo";
    EXPECT_FALSE(plan_add(before, &m, ergo).ok);
    AddRequest bad_features;
    bad_features.name     = "pictor";
    bad_features.features = "+bad value";
    EXPECT_FALSE(plan_add(before, &m, bad_features).ok);
    AddRequest short_rev = private_link("newlib");
    short_rev.rev = "3333333";
    EXPECT_FALSE(plan_add(before, &m, short_rev).ok);  // PKG-02
    EXPECT_FALSE(plan_add(fixture_text("ergo-manifest.json"), &m, private_link("newlib")).ok);  // not a host
}

TEST(PackageManagerRemove, RefusesToBreakNeeds) {
    const auto m = ergo_manifest();
    const std::string before = fixture_text("host-private.json");
    const auto refused = plan_remove(before, &m, "pictor");
    EXPECT_FALSE(refused.ok);
    EXPECT_NE(refused.error.find("ergo_render"), std::string::npos);
    EXPECT_FALSE(plan_remove(before, nullptr, "privlib").ok);  // needs cannot be checked
    EXPECT_FALSE(plan_remove(before, &m, "ergo").ok);
}

TEST(PackageManagerRemove, PrivatePackageLeavesUseAndPackages) {
    const auto m = ergo_manifest();
    const std::string before = fixture_text("host-private.json");
    const auto removed = plan_remove(before, &m, "privlib");
    ASSERT_TRUE(removed.ok);
    EXPECT_TRUE(removed.changed);
    EXPECT_NE(removed.text.find("\"use\": [\"pictor\"],"), std::string::npos);
    EXPECT_NE(removed.text.find("\"packages\": {}"), std::string::npos);
    const auto h = host_of(removed.text);
    EXPECT_TRUE(h.packages.empty());
    EXPECT_EQ(h.legacy_vars.size(), 2u);  // the legacy names point at ergo / pictor, not privlib

    const auto shared = plan_add(fixture_text("host-only.json"), &m, [] {
        AddRequest r;
        r.name = "tela";
        r.optional = true;
        return r;
    }());
    ASSERT_TRUE(shared.ok);
    const auto back = plan_remove(shared.text, &m, "tela");
    ASSERT_TRUE(back.ok);
    EXPECT_EQ(back.text, fixture_text("host-only.json"));

    const auto nothing = plan_remove(before, &m, "nosuch");
    ASSERT_TRUE(nothing.ok);
    EXPECT_FALSE(nothing.changed);
}
