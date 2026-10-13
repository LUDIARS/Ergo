// ergo_render_viz — Pictor の描画パイプライン定義を Tela の関係図ファイルに書き出す CLI。
//
//   ergo_render_viz --preset <名前> --out <file>
//   ergo_render_viz --profile <profile.json> [--preset <名前>] --out <file>
//
//   --preset   Pictor 組み込みプリセット (Low / Mid / High / Lite / Standard / Ultra 等)。
//              --profile と併用すると、 JSON に無い項目をこのプリセットの値で補う。
//   --profile  Pictor のパイプラインプロファイル JSON (例 Pictor/profiles/high.profile.json)。
//   --out      書き出す `TELA_GRAPH 1` ファイル。
//
// 書き出したファイルは Tela で開く:
//   tela_view --font <file.ttf> --graph <file>
//
// 計測値 (CPU 段・GPU 時間) は実行中のレンダラにしか無いので、 この CLI は構造だけを
// 描く。 時間付きの図はホストが `snapshot_from_pictor(profile, &stats)` で作る。

#include "ergo/render_viz/pictor_snapshot.h"
#include "ergo/render_viz/pipeline_layout.h"
#include "ergo/tela_graph/writer.h"

#include "pictor/pipeline/pipeline_profile.h"
#include "pictor/pipeline/pipeline_profile_serializer.h"

#include <cstdio>
#include <string>

namespace {

struct Args {
    std::string preset;
    std::string profile;
    std::string out;
};

void usage() {
    std::fprintf(stderr,
                 "usage: ergo_render_viz (--preset <name> | --profile <file.json> [--preset <name>]) "
                 "--out <file>\n");
}

bool parse(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) return false;
        const std::string value = argv[++i];
        if (key == "--preset")       args.preset  = value;
        else if (key == "--profile") args.profile = value;
        else if (key == "--out")     args.out     = value;
        else return false;
    }
    return !args.out.empty() && (!args.preset.empty() || !args.profile.empty());
}

void print_names(const pictor::PipelineProfileManager& manager) {
    std::fprintf(stderr, "available presets:");
    for (const auto& name : manager.profile_names()) std::fprintf(stderr, " %s", name.c_str());
    std::fprintf(stderr, "\n");
}

} // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse(argc, argv, args)) {
        usage();
        return 2;
    }

    pictor::PipelineProfileManager manager;
    manager.register_defaults();

    const pictor::PipelineProfileDef* preset = nullptr;
    if (!args.preset.empty()) {
        preset = manager.get_profile(args.preset);
        if (!preset) {
            std::fprintf(stderr, "unknown preset: %s\n", args.preset.c_str());
            print_names(manager);
            return 2;
        }
    }

    pictor::PipelineProfileDef profile;
    if (!args.profile.empty()) {
        std::string error;
        const bool ok = preset
            ? pictor::load_pipeline_profile_file(args.profile, *preset, profile, &error)
            : pictor::load_pipeline_profile_file(args.profile, profile, &error);
        if (!ok) {
            std::fprintf(stderr, "cannot load %s: %s\n", args.profile.c_str(), error.c_str());
            return 1;
        }
    } else {
        profile = *preset;
    }

    const auto graph = ergo::render_viz::layout_pipeline(
        ergo::render_viz::snapshot_from_pictor(profile));
    std::string error;
    if (!ergo::tela_graph::save_tela_graph(args.out, graph, &error)) {
        std::fprintf(stderr, "cannot write %s: %s\n", args.out.c_str(), error.c_str());
        return 1;
    }
    std::printf("wrote %s (%zu nodes, %zu edges)\n", args.out.c_str(),
                graph.nodes.size(), graph.edges.size());
    return 0;
}
