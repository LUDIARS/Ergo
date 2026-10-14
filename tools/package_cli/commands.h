#pragma once

// ergo_pkg のコマンド。 1 コマンド 1 ファイル (cmd_<name>.cpp)。
// 終了コードは ergo_package_viz と同じ: 0 成功 / 1 要対応の検出あり / 2 引数・入力の誤り /
// 3 書けない・取れない。

#include "cli_args.h"

namespace ergo::package_cli {

enum ExitCode : int {
    kExitOk       = 0,
    kExitFindings = 1,
    kExitInput    = 2,
    kExitIo       = 3,
};

int run_init(const Args& args);
int run_list(const Args& args);
int run_doctor(const Args& args);
int run_fetch(const Args& args);

} // namespace ergo::package_cli
