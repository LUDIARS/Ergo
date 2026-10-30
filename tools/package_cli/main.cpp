// ergo_pkg — Ergo のパッケージマネージャ CLI (spec/feature/package-manager.md §2.2、 段階 W1)。
//
//   ergo_pkg init   [--code <c>] [--upm]
//   ergo_pkg list   [--json] [--build <dir>]
//   ergo_pkg doctor [--strict] [--json]
//   ergo_pkg fetch  [<name>...] [--build <dir> | --cache <dir>]
//   ergo_pkg add    <name> [--optional] [--features <f>] [--kind ..] [--repository <url> --rev <r>]  (W2)
//   ergo_pkg remove <name>                                                                       (W2)
//   ergo_pkg update <name> --to <tag|main|sha> [--allow-off-main]                                (W2)
//   ergo_pkg outdated [--json] [--fail-on <kind>]                    (W3)
//   ergo_pkg watch  --host <dir>... [--state <dir>] [--out <dir>]    (W3)
//   ergo_pkg status --build <dir> [--junit <xml>] [--out <file>]     (W6)
//   ergo_pkg [<command>] --help    全コマンド / 1 コマンドの使い方 (cli_args.cpp のコマンドの表から)
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
        std::fprintf(stderr, "ergo_pkg: %s\n%s", error.c_str(), usage().c_str());
        return kExitInput;
    }
    if (args.help) {
        std::printf("%s", (args.command.empty() ? usage() : usage_for(args.command)).c_str());
        return kExitOk;
    }
    if (args.command == "init") return run_init(args);
    if (args.command == "list") return run_list(args);
    if (args.command == "doctor") return run_doctor(args);
    if (args.command == "add") return run_add(args);
    if (args.command == "remove") return run_remove(args);
    if (args.command == "update") return run_update(args);
    if (args.command == "status") return run_status(args);
#if defined(ERGO_PKG_CLI_WITH_WATCH)
    if (args.command == "outdated") return run_outdated(args);
    if (args.command == "watch") return run_watch(args);
#else
    if (args.command == "outdated" || args.command == "watch") {
        std::fprintf(stderr, "ergo_pkg: %s needs ergo_package_watch (ERGO_BUILD_PACKAGE_WATCH=ON)\n",
                     args.command.c_str());
        return kExitInput;
    }
#endif
    return run_fetch(args);
}
