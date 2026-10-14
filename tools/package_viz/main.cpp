// ergo_package_viz — パッケージ構成を Tela の関係図 (.tela) にする CLI。
//
//   ergo_package_viz build --view packages --out packages.tela
//   ergo_package_viz build --view targets --focus ergo --out targets.tela
//   ergo_package_viz build --check            # PKG-17 宣言漏れで終了コード 1
//   ergo_package_viz build --view usage --root <兄弟チェックアウトの親> --out usage.tela
//   ergo_package_viz build --scan --root <dir> # 観測の辺を根拠付きで表示
//   tela_view --font <file.ttf> --graph packages.tela
//
// 入力は構成時にハブが書いた <build>/ergo-packages.resolved.json と CMake File API の返答。
// usage / scan はマニフェストの observe が指す兄弟チェックアウトを読む (ergo_package_scan)。
// 終了コード: 0 成功 / 1 宣言漏れ (--check) / 2 引数・入力の誤り / 3 図にできない・書けない。

#include "cli_options.h"
#include "usage_command.h"
#include "version_probe.h"

#include "ergo/package_model/declaration_check.h"
#include "ergo/package_model/loader.h"
#include "ergo/package_viz/fit.h"
#include "ergo/package_viz/usage_view.h"
#include "ergo/tela_graph/writer.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace ergo;

int run_check(const package_viz::cli::Options& o, const package_model::PackageModel& model) {
    const auto links = package_model::find_undeclared_links(model, o.view.include_tests);
    for (const auto& l : links) std::printf("%s\n", package_model::describe(l).c_str());
    std::printf("ergo_package_viz: %zu undeclared package link(s)\n", links.size());
    return links.empty() ? 0 : 1;
}

/// どれか 1 枚でも検査に落ちるなら何も書かない (make_view が検査済みだが書く前にも確かめる)。
int write_result(const package_viz::FitResult& result, const std::string& out) {
    std::string error;
    const auto files = package_viz::output_files(result, out);
    for (const auto& [path, graph] : files) {
        if (!tela_graph::validate_tela_graph(*graph, &error)) {
            std::fprintf(stderr, "ergo_package_viz: %s: %s\n", path.c_str(), error.c_str());
            return 3;
        }
    }
    for (const auto& [path, graph] : files) {
        if (!tela_graph::save_tela_graph(path, *graph, &error)) {
            std::fprintf(stderr, "ergo_package_viz: %s\n", error.c_str());
            return 3;
        }
        std::printf("wrote %s (%zu nodes, %zu edges)\n", path.c_str(), graph->nodes.size(),
                    graph->edges.size());
    }
    return 0;
}

int run_view(const package_viz::cli::Options& o, package_model::PackageModel& model) {
    if (o.version_probe) package_viz::cli::probe_versions(model);

    package_viz::FitResult result;
    std::string error;
    if (!package_viz::make_view(model, o.view, result, &error)) {
        std::fprintf(stderr, "ergo_package_viz: %s\n", error.c_str());
        return 3;
    }
    return write_result(result, o.out);
}

int run_usage(const package_viz::cli::Options& o) {
    package_scan::UsageModel usage;
    std::string error;
    if (!package_viz::cli::scan_from_options(o, usage, &error)) {
        std::fprintf(stderr, "ergo_package_viz: %s\n", error.c_str());
        return 2;
    }
    if (o.scan) return package_viz::cli::print_scan(usage);
    for (const auto& note : usage.notes) std::fprintf(stderr, "ergo_package_viz: %s\n", note.c_str());
    package_viz::FitResult result;
    if (!package_viz::make_usage_view(usage, o.view, result, &error)) {
        std::fprintf(stderr, "ergo_package_viz: %s\n", error.c_str());
        return 3;
    }
    return write_result(result, o.out);
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    package_viz::cli::Options o;
    std::string error;
    if (!package_viz::cli::parse_options(args, o, &error)) {
        std::fprintf(stderr, "ergo_package_viz: %s\n%s", error.c_str(), package_viz::cli::usage());
        return 2;
    }
    if (o.help) {
        std::printf("%s", package_viz::cli::usage());
        return 0;
    }
    if (o.scan || (!o.check && o.view.view == package_viz::View::Usage)) return run_usage(o);

    package_model::LoadOptions load;
    load.build_dir          = o.build_dir;
    load.resolved_path      = o.resolved;
    load.layers_path        = o.layers;
    load.require_codemodel  = o.check || o.view.view == package_viz::View::Targets;
    load.codemodel.configuration = o.config;
    package_model::PackageModel model;
    package_model::LoadReport report;
    if (!package_model::load_package_model(load, model, &report, &error)) {
        std::fprintf(stderr, "ergo_package_viz: %s\n", error.c_str());
        return 2;
    }
    for (const auto& note : report.notes) std::fprintf(stderr, "ergo_package_viz: %s\n", note.c_str());

    return o.check ? run_check(o, model) : run_view(o, model);
}
