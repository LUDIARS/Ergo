// ergo_package_model: resolved.json / File API codemodel v2 (同梱の合成見本) の読み取り、
// 帰属・既定の除外・層・宣言漏れ (PKG-17)・版の札・jsonm の拡張。

#include "gtest/gtest.h"

#include "ergo/common/json_min.h"
#include "ergo/package_model/attribution.h"
#include "ergo/package_model/declaration_check.h"
#include "ergo/package_model/file_api_reader.h"
#include "ergo/package_model/layers.h"
#include "ergo/package_model/loader.h"
#include "ergo/package_model/resolved_reader.h"
#include "ergo/package_model/version_label.h"

#include <string>

using namespace ergo::package_model;

namespace {

const std::string kFixtures = ERGO_PACKAGE_MODEL_FIXTURES;
const std::string kHost     = kFixtures + "/fileapi_host";

PackageModel load_host(bool with_layers = true) {
    LoadOptions o;
    o.build_dir         = kHost;
    o.require_codemodel = true;
    if (with_layers) o.layers_path = kFixtures + "/layers.json";
    PackageModel m;
    std::string error;
    load_package_model(o, m, nullptr, &error);
    return m;
}

const Target* target(const PackageModel& m, const std::string& name) {
    for (const auto& t : m.targets) {
        if (t.name == name) return &t;
    }
    return nullptr;
}

} // namespace

TEST(PackageModel, JsonmDecodesSurrogatePairs) {
    ergo::common::jsonm::JsonValue v;
    ASSERT_TRUE(ergo::common::jsonm::parse("\"\\ud83d\\ude00/\\u00e9\"", v));
    EXPECT_EQ(v.s, std::string("\xF0\x9F\x98\x80/\xC3\xA9"));
}

TEST(PackageModel, ResolvedReaderReadsPackagesModulesObserve) {
    PackageModel m;
    std::string error;
    ASSERT_TRUE(read_resolved_file(kHost + "/ergo-packages.resolved.json", m, &error));
    EXPECT_EQ(m.mode, std::string("M1"));
    EXPECT_EQ(m.platform, std::string("desktop"));
    const auto* pictor = m.find_package("pictor");
    ASSERT_TRUE(pictor != nullptr);
    EXPECT_TRUE(pictor->kind == PackageKind::Link);
    EXPECT_EQ(pictor->group, std::string("render"));
    EXPECT_EQ(pictor->drift, std::string("ahead"));
    EXPECT_TRUE(pictor->dirty);
    EXPECT_EQ(pictor->source_dir, std::string("/work/Pictor"));
    EXPECT_EQ(pictor->version.text, std::string("2.2.0"));
    EXPECT_EQ(pictor->version.source, std::string("cmake"));
    const auto* tela = m.find_package("tela");
    ASSERT_TRUE(tela != nullptr);
    ASSERT_EQ(tela->requires_.size(), 1u);
    EXPECT_EQ(tela->requires_[0], std::string("pictor"));
    const auto* lapilli = m.find_package("lapilli");
    ASSERT_TRUE(lapilli != nullptr);
    EXPECT_TRUE(lapilli->kind == PackageKind::Tool);
    EXPECT_FALSE(lapilli->initialized);
    const auto* clio = m.find_package("clio");
    ASSERT_TRUE(clio != nullptr);
    EXPECT_TRUE(clio->kind == PackageKind::Observe);
    ASSERT_EQ(m.observe.size(), 2u);
    ASSERT_EQ(m.observe[1].relations.size(), 1u);
    EXPECT_EQ(m.observe[1].relations[0].from, std::string("ergo_resource"));
    EXPECT_EQ(m.observe[1].relations[0].kind, std::string("shares-vocabulary"));
    ASSERT_EQ(m.findings.size(), 1u);
    EXPECT_EQ(m.findings[0].id, std::string("PKG-05"));
}

TEST(PackageModel, ResolvedReaderRejectsOtherSchemas) {
    PackageModel m;
    std::string error;
    EXPECT_FALSE(read_resolved_text("{\"schema\": 2, \"packages\": {}}", m, &error));
    EXPECT_TRUE(error.find("schema") != std::string::npos);
    EXPECT_FALSE(read_resolved_text("{not json", m, &error));
}

TEST(PackageModel, CodemodelReadsLatestIndexAndSkipsGeneratorTargets) {
    PackageModel m;
    std::string error;
    ASSERT_TRUE(read_codemodel(kHost, m, &error));
    EXPECT_TRUE(m.has_codemodel);
    EXPECT_EQ(m.configuration, std::string("Debug"));
    EXPECT_EQ(m.source_root, std::string("/work/demo"));
    EXPECT_EQ(m.ergo_root, std::string("/work/demo/external/ergo"));
    EXPECT_EQ(m.targets.size(), 11u);
    EXPECT_TRUE(target(m, "ALL_BUILD") == nullptr);
    const auto* render = target(m, "ergo_render");
    ASSERT_TRUE(render != nullptr);
    EXPECT_EQ(render->type, std::string("STATIC_LIBRARY"));
    EXPECT_EQ(render->project, std::string("Ergo"));
    EXPECT_EQ(render->dependencies.size(), 2u);
}

TEST(PackageModel, CodemodelRejectsUnsupportedVersion) {
    PackageModel m;
    std::string error;
    EXPECT_FALSE(read_codemodel(kFixtures + "/fileapi_v3", m, &error));
    EXPECT_TRUE(error.find("unsupported codemodel version 3.0") != std::string::npos);
    EXPECT_FALSE(read_codemodel(kFixtures + "/no_such_build", m, &error));
    EXPECT_TRUE(error.find("no reply index") != std::string::npos);
}

TEST(PackageModel, AttributionByLongestSourceDir) {
    const auto m = load_host();
    ASSERT_TRUE(m.has_codemodel);
    EXPECT_EQ(target(m, "demo_app")->package, std::string("host"));
    EXPECT_EQ(target(m, "ergo_render")->package, std::string("ergo"));
    EXPECT_EQ(target(m, "pictor")->package, std::string("pictor"));
    // ビルドツリー (ソースの下) に取得した第三者は ergo / host に数えない。
    EXPECT_EQ(target(m, "glfw")->package, std::string("glfw"));
    ASSERT_TRUE(m.find_package("glfw") != nullptr);
    EXPECT_TRUE(m.find_package("glfw")->kind == PackageKind::ThirdParty);
    ASSERT_TRUE(m.find_package("host") != nullptr);
    ASSERT_TRUE(m.find_package("ergo") != nullptr);
}

TEST(PackageModel, DefaultExclusions) {
    const auto m = load_host();
    EXPECT_TRUE(target(m, "test_render")->excluded);
    EXPECT_TRUE(target(m, "ergo_gtest_main")->excluded);
    EXPECT_TRUE(target(m, "ergo_input_dummy")->excluded);
    EXPECT_FALSE(target(m, "ergo_render")->excluded);
    EXPECT_FALSE(target(m, "demo_app")->excluded);
    EXPECT_TRUE(excluded_by_default("bench_curve", {}));
    EXPECT_TRUE(excluded_by_default("runner", {"tests/a.cpp", "tests/b.cpp"}));
    EXPECT_FALSE(excluded_by_default("runner", {"tests/a.cpp", "src/b.cpp"}));
}

TEST(PackageModel, GlobsMatchLikeAnatomia) {
    EXPECT_TRUE(glob_match("src/render/*", "src/render/a.cpp"));
    EXPECT_FALSE(glob_match("src/render/*", "src/render/sub/a.cpp"));
    EXPECT_TRUE(glob_match("tests/**", "tests/render/sub/a.cpp"));
    EXPECT_FALSE(glob_match("src/*", "include/a.h"));
    std::vector<LayerRule> rules;
    ASSERT_TRUE(read_layers_text(
        "{\"layers\":[{\"glob\":\"src/b/*\",\"layer\":\"shared\"},"
        "{\"glob\":\"src/a/*\",\"layer\":\"presentation\"}]}", rules));
    ASSERT_EQ(rules.size(), 2u);
    EXPECT_EQ(rules[0].glob, std::string("src/a/*"));   // glob 順
    EXPECT_EQ(layer_of(rules, "src/b/x.cpp"), std::string("shared"));
    EXPECT_EQ(layer_of(rules, "src/c/x.cpp"), std::string());
}

TEST(PackageModel, LayersAssignedToErgoTargets) {
    const auto m = load_host();
    EXPECT_EQ(target(m, "ergo_render")->layer, std::string("presentation"));
    EXPECT_EQ(target(m, "ergo_input")->layer, std::string("infrastructure"));
    EXPECT_EQ(target(m, "ergo_common")->layer, std::string("shared"));
    EXPECT_EQ(target(m, "demo_app")->layer, std::string());   // host には当てない
    // 層宣言が無い (既定の場所に無い) ときは未分類のまま読む。
    const auto bare = load_host(false);
    ASSERT_TRUE(bare.has_codemodel);
    EXPECT_EQ(target(bare, "ergo_render")->layer, std::string());
}

TEST(PackageModel, UndeclaredLinksAreDetected) {
    const auto m = load_host();
    const auto links = find_undeclared_links(m);
    ASSERT_EQ(links.size(), 1u);
    EXPECT_EQ(links[0].from_target, std::string("ergo_gpu_particle"));
    EXPECT_EQ(links[0].to_package, std::string("pictor"));
    EXPECT_TRUE(describe(links[0]).find("PKG-17") == 0);
    // テストを戻すと test_render → pictor も漏れ。
    EXPECT_EQ(find_undeclared_links(m, true).size(), 2u);
    // ergo_particle_pictor は ergo_particle の optional で宣言済み。
    EXPECT_EQ(module_for_target(m, "ergo_particle_pictor"), std::string("ergo_particle"));
    EXPECT_EQ(module_for_target(m, "ergo_render_viz_cli"), std::string("ergo_render_viz"));
    EXPECT_EQ(module_for_target(m, "ergo_gpu_particle"), std::string());
}

TEST(PackageModel, LinkPackageRequiresDeclareLinks) {
    auto m = load_host();
    // pictor → tela の実リンクを足す: pictor の requires に無ければ漏れ、 足せば宣言済み。
    Target t;
    t.id = "tela_core::@x";
    t.name = "tela_core";
    t.type = "STATIC_LIBRARY";
    t.package = "tela";
    m.targets.push_back(t);
    for (auto& tg : m.targets) {
        if (tg.name == "pictor") tg.dependencies.push_back("tela_core::@x");
    }
    EXPECT_EQ(find_undeclared_links(m).size(), 2u);
    for (auto& p : m.packages) {
        if (p.name == "pictor") p.requires_.push_back("tela");
    }
    EXPECT_EQ(find_undeclared_links(m).size(), 1u);
}

TEST(PackageModel, LoaderWithoutCodemodelStillReadsResolved) {
    LoadOptions o;
    o.build_dir     = kFixtures + "/no_such_build";
    o.resolved_path = kHost + "/ergo-packages.resolved.json";
    PackageModel m;
    LoadReport report;
    std::string error;
    ASSERT_TRUE(load_package_model(o, m, &report, &error));
    EXPECT_FALSE(m.has_codemodel);
    EXPECT_TRUE(m.find_package("ergo") != nullptr);
    EXPECT_EQ(report.notes.size(), 1u);
    o.require_codemodel = true;
    EXPECT_FALSE(load_package_model(o, m, &report, &error));
}

TEST(PackageModel, VersionLabels) {
    EXPECT_EQ(version_from_describe("v2.2.0-12-g625d83d\n"), std::string("2.2.0+12"));
    EXPECT_EQ(version_from_describe("v2.2.0-0-g625d83d"), std::string("2.2.0"));
    EXPECT_EQ(version_from_describe("release-3-4-gabc1234-dirty"), std::string("release-3+4"));
    EXPECT_EQ(version_from_describe("625d83d"), std::string());
    EXPECT_EQ(version_from_describe(""), std::string());
    EXPECT_EQ(version_from_revisor("Pictor version: 2.3.0 (local)\n"), std::string("2.3.0"));
    EXPECT_EQ(version_from_revisor("uninitialized"), std::string());
    EXPECT_EQ(version_from_revisor("build 12.5 then 1.2.3"), std::string("1.2.3"));
    EXPECT_EQ(version_line({"2.2.0+12", "git"}), std::string("版 2.2.0+12 [git]"));
    EXPECT_EQ(version_line({}), std::string("版 不明"));
    EXPECT_EQ(short_sha("625d83d61c90c30ceed3fab2bd2e11a7edf8d39f"), std::string("625d83d"));
}

TEST(PackageModel, PathHelpers) {
    EXPECT_EQ(normalize_path("src/../include/a.h", "/work/x"), std::string("/work/x/include/a.h"));
    EXPECT_EQ(normalize_path("C:\\Work\\X\\"), std::string("C:/Work/X"));
    EXPECT_TRUE(path_within("/work/x/y", "/work/x"));
    EXPECT_TRUE(path_within("/work/x", "/work/x"));
    EXPECT_FALSE(path_within("/work/xy", "/work/x"));
    EXPECT_TRUE(path_within("c:/Work/X/src", "C:/work/x"));   // ドライブ付きは大小を区別しない
}
