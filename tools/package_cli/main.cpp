// ergo_pkg — Ergo のパッケージマネージャ CLI (spec/feature/package-manager.md §2.2、 段階 W1)。
//
//   ergo_pkg init   [--code <c>] [--upm]
//   ergo_pkg list   [--json] [--build <dir>]
//   ergo_pkg doctor [--strict] [--json]
//   ergo_pkg fetch  [<name>...] [--build <dir> | --cache <dir>]
//
// main は引数の解析とコマンドへの振り分けだけ。 終了コード: 0 成功 / 1 要対応の検出あり /
// 2 引数・入力の誤り / 3 書けない・取れない。 コミット・ブランチ作成・PR はしない。

#include "cli_args.h"
#include "commands.h"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace ergo::package_cli;
    const std::vector<std::string> raw(argv + 1, argv + argc);
    Args args;
    std::string error;
    if (!parse_args(raw, args, &error)) {
        std::fprintf(stderr, "ergo_pkg: %s\n%s", error.c_str(), usage());
        return kExitInput;
    }
    if (args.help) {
        std::printf("%s", usage());
        return kExitOk;
    }
    if (args.command == "init") return run_init(args);
    if (args.command == "list") return run_list(args);
    if (args.command == "doctor") return run_doctor(args);
    return run_fetch(args);
}
