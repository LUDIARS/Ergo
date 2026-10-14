#pragma once

// add / remove / update の共通部品: 版の解決 (git ミラー経由) と、 書き換え結果の書き出し・表示。

#include "cli_args.h"

#include "ergo/package_manager/declaration_edit.h"
#include "ergo/package_manager/rev_resolution.h"

#include <string>

namespace ergo::package_cli {

/// repository の to を解決する。 失敗なら終了コード (2 / 3) を返し、 成功なら 0 と out。 警告は stderr に出す。
int resolve_with_git(const char* command, const Args& args, const std::string& repository,
                     const std::string& to, ergo::package_manager::RevResolution& out);

/// 書き換え結果を path に書く (変化が無ければ書かない)。 誤りなら 2、 書けなければ 3、 それ以外 0。
int write_edit(const char* command, const std::string& path, const ergo::package_manager::EditResult& edit);

} // namespace ergo::package_cli
