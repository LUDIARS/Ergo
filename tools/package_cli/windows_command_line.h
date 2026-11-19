#pragma once

// CreateProcessW に渡すコマンドラインを argv から組み立てる (CommandLineToArgvW / MSVC CRT の規則)。
// シェル (cmd.exe) は通らないので、 & | < > ^ % などはただの文字として子に届く。
// どの OS でもコンパイルでき、 規則そのものを単体テストできる。

#include <string>
#include <vector>

namespace ergo::package_cli {

/// 1 個の引数を、 子の CRT が元の 1 個に戻せる形にクォートする。
std::string quote_windows_arg(const std::string& arg);

/// argv の各要素をクォートして空白でつなぐ。
std::string windows_command_line(const std::vector<std::string>& argv);

} // namespace ergo::package_cli
