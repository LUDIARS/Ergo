// `ergo_pkg list` の一覧: 宣言・Ergo のマニフェスト・resolved.json・UPM の宣言を合わせる。

#include "fixture_util.h"

#include "ergo/package_manager/package_list.h"

#include "gtest/gtest.h"

using namespace ergo::package_manager;
using ergo::package_manager::test::fixture_json;
namespace jsonm = ergo::common::jsonm;

namespace {

struct Inputs {
    HostDeclaration host;
    ErgoManifest    ergo;
};

Inputs load() {
    Inputs in;
    std::vector<Finding> read;
    read_host_declaration(fixture_json("host-private.json"), in.host, read);
    std::string error;
    read_ergo_manifest(fixture_json("ergo-manifest.json"), in.ergo, &error);
    return in;
}

} // namespace

TEST(PackageManagerList, OrderOwnersAndSelection) {
    auto in = load();
    jsonm::JsonValue resolved;
    ASSERT_TRUE(jsonm::parse(R"({"schema":1,"packages":{"pictor":{"route":"source","state":"hub-managed"}}})",
                             resolved));
    ListInput li;
    li.host     = &in.host;
    li.ergo     = &in.ergo;
    li.resolved = &resolved;
    const auto rows = make_list(li);
    ASSERT_EQ(rows.size(), static_cast<std::size_t>(5));
    EXPECT_EQ(rows[0].name, std::string("ergo"));
    EXPECT_EQ(rows[0].owner, std::string("host"));
    EXPECT_EQ(rows[1].name, std::string("lapilli"));
    EXPECT_EQ(rows[2].name, std::string("pictor"));
    EXPECT_EQ(rows[2].owner, std::string("ergo"));
    EXPECT_TRUE(rows[2].used);
    EXPECT_TRUE(rows[2].optional);
    EXPECT_EQ(rows[2].features, std::string("+vulkan"));
    EXPECT_EQ(rows[2].route, std::string("source"));
    EXPECT_FALSE(rows[3].used);  // tela
    EXPECT_EQ(rows[4].name, std::string("privlib"));
    EXPECT_EQ(rows[4].owner, std::string("host"));

    const auto text = render_list_text(rows);
    EXPECT_TRUE(text.find("privlib") != std::string::npos);
    EXPECT_EQ(jsonm::serialize(list_to_json(rows, "Example")),
              jsonm::serialize(list_to_json(make_list(li), "Example")));
}

TEST(PackageManagerList, WorksWithMissingInputs) {
    ListInput li;
    EXPECT_TRUE(make_list(li).empty());
    EXPECT_FALSE(render_list_text({}).empty());  // header only
}

TEST(PackageManagerList, UpmDependencies) {
    jsonm::JsonValue manifest;
    ASSERT_TRUE(jsonm::parse(R"({"dependencies":{
        "com.ludiars.tela":"https://github.com/LUDIARS/Tela.git?path=/unity/com.ludiars.tela#main",
        "com.ludiars.ergo.packages":"https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages#625d83d61c90c30ceed3fab2bd2e11a7edf8d39f",
        "com.unity.ugui":"1.0.0"}})", manifest));
    UpmDeclaration upm;
    upm.dependencies = read_upm_dependencies(manifest);
    ASSERT_EQ(upm.dependencies.size(), static_cast<std::size_t>(2));
    EXPECT_FALSE(upm.dependencies[0].branch_ref);  // com.ludiars.ergo.packages
    EXPECT_TRUE(upm.dependencies[1].branch_ref);   // #main
    jsonm::JsonValue host_json;
    ASSERT_TRUE(jsonm::parse(R"({"schema":1,"code":"Example"})", host_json));
    EXPECT_EQ(read_upm_host_code(host_json), std::string("Example"));

    ListInput li;
    li.upm = &upm;
    const auto rows = make_list(li);
    ASSERT_EQ(rows.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(rows[1].state, std::string("branch-ref"));
}
