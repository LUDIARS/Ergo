// 漏れ検査 (spec/feature/package-manager.md §8) を真偽両方で確かめる。

#include "ergo/package_manager/leak_check.h"

#include "gtest/gtest.h"

using namespace ergo::package_manager;
namespace jsonm = ergo::common::jsonm;

TEST(PackageManagerLeak, LocalPathShapes) {
    EXPECT_TRUE(looks_like_local_path("C:/work/Ergo"));
    EXPECT_TRUE(looks_like_local_path("d:\\src\\Pictor"));
    EXPECT_TRUE(looks_like_local_path("/Users/someone/src"));
    EXPECT_TRUE(looks_like_local_path("/home/someone/src"));
    EXPECT_TRUE(looks_like_local_path("\\\\server\\share"));

    EXPECT_FALSE(looks_like_local_path("https://github.com/LUDIARS/Ergo.git"));
    EXPECT_FALSE(looks_like_local_path("cmake/packages/pictor.cmake"));
    EXPECT_FALSE(looks_like_local_path("<ergo>/cmake"));
    EXPECT_FALSE(looks_like_local_path("ergo.source_dir"));
    EXPECT_FALSE(looks_like_local_path("C:"));
}

TEST(PackageManagerLeak, SecretKeys) {
    EXPECT_TRUE(is_secret_key("token"));
    EXPECT_TRUE(is_secret_key("API_TOKEN"));
    EXPECT_TRUE(is_secret_key("client_secret"));
    EXPECT_TRUE(is_secret_key("Authorization"));
    EXPECT_FALSE(is_secret_key("repository"));
    EXPECT_FALSE(is_secret_key("rev"));
}

TEST(PackageManagerLeak, CleanDeclarationHasNoLeak) {
    jsonm::JsonValue doc;
    ASSERT_TRUE(jsonm::parse(R"({"schema":1,"host":{"code":"Example",
        "ergo":{"repository":"https://github.com/LUDIARS/Ergo.git","rev":"625d83d61c90c30ceed3fab2bd2e11a7edf8d39f"},
        "legacy_vars":{"EXAMPLE_ERGO_DIR":"ergo.source_dir"}},"packages":{}})", doc));
    EXPECT_TRUE(find_leaks(doc).empty());
}

TEST(PackageManagerLeak, FindsPathsAndSecretsWithPointers) {
    jsonm::JsonValue doc;
    ASSERT_TRUE(jsonm::parse(R"({"host":{"code":"Example",
        "paths":["/home/someone/Ergo"],
        "auth":{"token":"abc"},
        "legacy_vars":{"X":"Z:/work/Ergo"}}})", doc));
    const auto leaks = find_leaks(doc);
    ASSERT_EQ(leaks.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(leaks[0].where, std::string("/host/auth/token"));
    EXPECT_EQ(leaks[1].where, std::string("/host/legacy_vars/X"));
    EXPECT_EQ(leaks[2].where, std::string("/host/paths/0"));
    const auto findings = leak_findings(leaks, "ergo-packages.json");
    ASSERT_EQ(findings.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(findings[0].id, std::string("LEAK"));
    EXPECT_TRUE(findings[0].severity == Severity::Error);
}

TEST(PackageManagerLeak, TextLeaks) {
    EXPECT_TRUE(find_text_leaks("set(ERGO_SOURCE_DIR \"\" CACHE PATH \"x\")\n").empty());
    const auto leaks = find_text_leaks("line one\nset(X \"C:/work/Ergo\")\n");
    ASSERT_EQ(leaks.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(leaks[0].where, std::string("line 2"));
}
