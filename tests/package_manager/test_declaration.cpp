// ホストの宣言の読み取りと検査 (spec/feature/package-manager.md §7 W1:
// host 節の見本 3 種 (host のみ / 非公開 packages あり / 不正) の読み取りと拒否理由、
// host 節のファイルを Ergo マニフェストとして読むと誤り、 非公開名と Ergo マニフェストの同名で PKG-15)。

#include "fixture_util.h"

#include "ergo/package_manager/declaration_check.h"
#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/host_declaration.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdio>

using namespace ergo::package_manager;
using ergo::package_manager::test::fixture_json;

namespace {

ErgoManifest load_ergo() {
    ErgoManifest m;
    std::string error;
    read_ergo_manifest(fixture_json("ergo-manifest.json"), m, &error);
    return m;
}

bool has(const std::vector<Finding>& fs, const std::string& id, Severity sev, const std::string& subject) {
    return std::any_of(fs.begin(), fs.end(), [&](const Finding& f) {
        return f.id == id && f.severity == sev && f.subject == subject;
    });
}

} // namespace

TEST(PackageManagerManifest, ReadsNamesOfTheErgoManifest) {
    const auto m = load_ergo();
    EXPECT_EQ(m.schema, 1);
    EXPECT_EQ(m.packages.size(), static_cast<std::size_t>(3));
    ASSERT_TRUE(m.find("pictor") != nullptr);
    EXPECT_EQ(m.find("pictor")->rev, std::string("625d83d61c90c30ceed3fab2bd2e11a7edf8d39f"));
    EXPECT_TRUE(m.has_name("ergo_render"));
    EXPECT_TRUE(m.has_name("curare"));
    EXPECT_FALSE(m.has_name("privlib"));
}

TEST(PackageManagerManifest, HostDeclarationIsNotAnErgoManifest) {
    ErgoManifest m;
    std::string error;
    EXPECT_FALSE(read_ergo_manifest(fixture_json("host-only.json"), m, &error));
    EXPECT_TRUE(error.find("host declaration") != std::string::npos);
}

TEST(PackageManagerDeclaration, HostOnly) {
    HostDeclaration h;
    std::vector<Finding> read;
    ASSERT_TRUE(read_host_declaration(fixture_json("host-only.json"), h, read));
    EXPECT_TRUE(read.empty());
    EXPECT_EQ(h.code, std::string("Example"));
    EXPECT_TRUE(h.ergo.present);
    EXPECT_EQ(h.ergo.expect_tag, std::string("v0.9.0"));
    EXPECT_TRUE(h.use.empty());
    EXPECT_TRUE(h.packages.empty());
    const auto m = load_ergo();
    EXPECT_TRUE(check_declaration(h, &m).empty());
}

TEST(PackageManagerDeclaration, PrivatePackages) {
    HostDeclaration h;
    std::vector<Finding> read;
    ASSERT_TRUE(read_host_declaration(fixture_json("host-private.json"), h, read));
    EXPECT_TRUE(read.empty());
    ASSERT_EQ(h.packages.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(h.packages[0].name, std::string("privlib"));
    EXPECT_EQ(h.use.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(h.features["pictor"], std::string("+vulkan"));
    EXPECT_EQ(h.legacy_vars["EXAMPLE_ERGO_DIR"], std::string("ergo.source_dir"));
    const auto m = load_ergo();
    const auto findings = check_declaration(h, &m);
    for (const auto& f : findings) std::printf("unexpected: %s\n", describe(f).c_str());
    EXPECT_TRUE(findings.empty());
}

TEST(PackageManagerDeclaration, InvalidIsRejectedWithReasons) {
    HostDeclaration h;
    std::vector<Finding> read;
    ASSERT_TRUE(read_host_declaration(fixture_json("host-invalid.json"), h, read));
    EXPECT_TRUE(has(read, "MANIFEST", Severity::Error, "host"));  // optional is not an array

    const auto m = load_ergo();
    const auto f = check_declaration(h, &m);
    EXPECT_TRUE(has(f, "PKG-02", Severity::Error, "ergo"));       // rev "main"
    EXPECT_TRUE(has(f, "PKG-01", Severity::Error, "nope"));       // unknown use
    EXPECT_TRUE(has(f, "PKG-15", Severity::Error, "pictor"));     // private reuses a public name
    EXPECT_TRUE(has(f, "MANIFEST", Severity::Error, "pictor"));   // feature request "+vulkan;;"
    EXPECT_TRUE(has(f, "MANIFEST", Severity::Error, "bad-name")); // legacy var name and target
}

TEST(PackageManagerDeclaration, NotAHostDeclaration) {
    HostDeclaration h;
    std::vector<Finding> read;
    EXPECT_FALSE(read_host_declaration(fixture_json("ergo-manifest.json"), h, read));
    EXPECT_TRUE(has(read, "MANIFEST", Severity::Error, "host"));
}

TEST(PackageManagerDeclaration, WithoutErgoManifestSkipsNameRules) {
    HostDeclaration h;
    std::vector<Finding> read;
    read_host_declaration(fixture_json("host-invalid.json"), h, read);
    const auto f = check_declaration(h, nullptr);
    EXPECT_FALSE(has(f, "PKG-15", Severity::Error, "pictor"));
    EXPECT_FALSE(has(f, "PKG-01", Severity::Error, "nope"));
    EXPECT_TRUE(has(f, "PKG-02", Severity::Error, "ergo"));
}

TEST(PackageManagerDeclaration, MissingErgoPinWarnsAndStrictPromotes) {
    ergo::common::jsonm::JsonValue doc;
    ergo::common::jsonm::parse(R"({"schema":1,"host":{"code":"Example"},"packages":{}})", doc);
    HostDeclaration h;
    std::vector<Finding> read;
    ASSERT_TRUE(read_host_declaration(doc, h, read));
    auto f = check_declaration(h, nullptr);
    EXPECT_TRUE(has(f, "MANIFEST", Severity::Warning, "ergo"));
    EXPECT_FALSE(has_severity(f, Severity::Error));
    promote_warnings(f);
    EXPECT_TRUE(has(f, "MANIFEST", Severity::Error, "ergo"));
}

TEST(PackageManagerDeclaration, FullSha) {
    EXPECT_TRUE(is_full_sha("625d83d61c90c30ceed3fab2bd2e11a7edf8d39f"));
    EXPECT_FALSE(is_full_sha("625d83d"));
    EXPECT_FALSE(is_full_sha("625D83D61C90C30CEED3FAB2BD2E11A7EDF8D39F"));
    EXPECT_FALSE(is_full_sha("main"));
}

TEST(PackageManagerDeclaration, FindingsAreSortedAndFormatted) {
    std::vector<Finding> f = {{"PKG-15", Severity::Error, "b", "x"},
                              {"PKG-01", Severity::Error, "a", "y"},
                              {"PKG-01", Severity::Error, "a", "y"}};
    sort_findings(f);
    ASSERT_EQ(f.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(describe(f[0]), std::string("PKG-01|error|a|y"));
}
