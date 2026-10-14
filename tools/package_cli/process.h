#pragma once

// ergo_pkg の外部プロセス起動 (git と cmake だけ)。 シェルを経由するので、 渡す値は quote_safe で確かめる。

#include <string>
#include <vector>

namespace ergo::package_cli {

/// 二重引用符で包んでも壊れない値か (引用符・改行・% ` $ を含まない、 空でない)。
bool quote_safe(const std::string& value);

/// 標準出力を out に取る (標準エラーは捨てる)。 終了コードが 0 でなければ false。
bool capture(const std::vector<std::string>& argv, std::string& out);

/// 出力をそのまま流して実行し、 終了コードが 0 なら true。
bool run(const std::vector<std::string>& argv);

/// 出力を捨てて実行し、 終了コードを返す (起動できない・値が危ういなら -1)。
int status(const std::vector<std::string>& argv);

/// `git -C <dir> rev-parse HEAD` (40 桁)。 取れなければ ""。
std::string git_head(const std::string& dir);

} // namespace ergo::package_cli
