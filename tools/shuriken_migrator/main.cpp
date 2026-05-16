/// Shuriken Migrator CLI
///
/// 使い方:
///   shuriken_migrator <input.prefab> [output.json]
///   shuriken_migrator --gpu <input.prefab> [output.json]
///
/// `--gpu` を指定すると gpu_particle::EmitterDescriptor の JSON 配列を出力する
/// (Shuriken のフル機能 — Burst / Velocity / Noise / Trail / SubEmitter /
///  TextureSheetAnimation 等)。 無指定は従来の 2D ParticleEffectConfig 出力。

#include "ergo/shuriken_migrator/migrator.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    bool gpu  = false;
    bool tree = false;
    int  arg  = 1;
    while (argc > arg && argv[arg][0] == '-') {
        if (std::strcmp(argv[arg], "--gpu") == 0)       gpu  = true;
        else if (std::strcmp(argv[arg], "--tree") == 0) tree = true;
        else break;
        ++arg;
    }
    if (argc <= arg) {
        std::fprintf(stderr,
            "usage: %s [--gpu] [--tree] <input.prefab> [output.json]\n"
            "  --gpu   gpu_particle::EmitterDescriptor を出力\n"
            "  --tree  子オブジェクトを探索し、 全 emitter をツリー構造を保ったまま出力\n",
            argv[0]);
        return 2;
    }
    std::string input  = argv[arg];
    std::string output = (argc > arg + 1) ? argv[arg + 1] : "";

    ergo::shuriken_migrator::MigrationReport report;
    std::string json = tree
        ? ergo::shuriken_migrator::MigrateFileToGpuTreeJson(input, report)
        : gpu
            ? ergo::shuriken_migrator::MigrateFileToGpuJson(input, report)
            : ergo::shuriken_migrator::MigrateFileToJson(input, report);

    if (json.empty()) {
        std::fprintf(stderr, "[shuriken_migrator] migration failed for %s\n", input.c_str());
        for (auto& w : report.warnings)    std::fprintf(stderr, "  warn: %s\n", w.c_str());
        for (auto& u : report.unsupported) std::fprintf(stderr, "  unsupp: %s\n", u.c_str());
        return 1;
    }

    if (output.empty()) {
        std::cout << json << "\n";
    } else {
        std::ofstream out(output);
        if (!out) {
            std::fprintf(stderr, "[shuriken_migrator] cannot open output: %s\n", output.c_str());
            return 1;
        }
        out << json;
    }

    std::fprintf(stderr,
        "[shuriken_migrator] %s: target=%s extracted=%d converted=%d warnings=%zu unsupported=%zu\n",
        input.c_str(), tree ? "gpu-tree" : gpu ? "gpu" : "2d",
        report.extractedSystems, report.convertedSystems,
        report.warnings.size(), report.unsupported.size());
    for (auto& w : report.warnings)    std::fprintf(stderr, "  warn: %s\n", w.c_str());
    for (auto& u : report.unsupported) std::fprintf(stderr, "  unsupp: %s\n", u.c_str());
    return 0;
}
