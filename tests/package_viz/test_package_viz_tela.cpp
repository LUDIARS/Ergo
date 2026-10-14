// Tela がある構成だけでビルドされる契約テスト。 ergo_package_viz が書いた P1 / P2 を
// Tela 本体の読み込み (tela::load_graph) が受け付け、 プロセス内変換と同じ図になること。

#include "gtest/gtest.h"

#include "ergo/package_model/loader.h"
#include "ergo/package_viz/fit.h"
#include "ergo/tela_graph/tela_adapter.h"
#include "ergo/tela_graph/writer.h"

#include <filesystem>
#include <string>

using namespace ergo;

namespace {

void expect_tela_accepts(package_viz::View view, const char* file) {
    package_model::LoadOptions lo;
    lo.build_dir         = std::string(ERGO_PACKAGE_MODEL_FIXTURES) + "/fileapi_host";
    lo.layers_path       = std::string(ERGO_PACKAGE_MODEL_FIXTURES) + "/layers.json";
    lo.require_codemodel = true;
    package_model::PackageModel model;
    std::string error;
    ASSERT_TRUE(package_model::load_package_model(lo, model, nullptr, &error));

    package_viz::ViewOptions o;
    o.view = view;
    package_viz::FitResult r;
    ASSERT_TRUE(package_viz::make_view(model, o, r, &error));

    const auto dir = std::filesystem::temp_directory_path() / "ergo_package_viz_tela_test";
    std::filesystem::create_directories(dir);
    const auto path = dir / file;
    ASSERT_TRUE(tela_graph::save_tela_graph(path.string(), r.graphs[0], &error));
    const tela::Graph loaded    = tela::load_graph(path);
    const tela::Graph converted = tela_graph::to_tela_graph(r.graphs[0]);
    EXPECT_TRUE(loaded.info() == converted.info());
    EXPECT_TRUE(loaded.groups() == converted.groups());
    EXPECT_TRUE(loaded.nodes() == converted.nodes());
    ASSERT_EQ(loaded.edges().size(), converted.edges().size());
    for (std::size_t i = 0; i < loaded.edges().size(); ++i) {
        const auto& a = loaded.edges()[i];
        const auto& b = converted.edges()[i];
        EXPECT_TRUE(a.id == b.id && a.from == b.from && a.to == b.to && a.dashed == b.dashed);
        ASSERT_EQ(a.points.size(), b.points.size());
        for (std::size_t k = 0; k < a.points.size(); ++k) {
            // ファイルは小数 2 桁で丸める (経路は 1/4 px なので実際は一致する)。
            EXPECT_NEAR(a.points[k].x, b.points[k].x, 0.006f);
            EXPECT_NEAR(a.points[k].y, b.points[k].y, 0.006f);
        }
    }
    std::filesystem::remove(path);
}

} // namespace

TEST(PackageVizTela, TelaLoadsPackagesView) {
    expect_tela_accepts(package_viz::View::Packages, "packages.tela");
}

TEST(PackageVizTela, TelaLoadsTargetsView) {
    expect_tela_accepts(package_viz::View::Targets, "targets.tela");
}
