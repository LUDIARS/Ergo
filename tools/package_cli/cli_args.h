#pragma once

// ergo_pkg CLI の引数。 解析だけを持つ (実行は cmd_*.cpp)。
//
//   ergo_pkg init   [--code <c>] [--upm]                 [--host <dir>] [--ergo <dir>]
//   ergo_pkg list   [--json] [--build <dir>]              [--host <dir>] [--ergo <dir>]
//   ergo_pkg doctor [--strict] [--json]                   [--host <dir>] [--ergo <dir>]
//   ergo_pkg fetch  [<name>...] [--build <dir> | --cache <dir>] [--cmake <exe>]
//                                                         [--host <dir>] [--ergo <dir>]

#include <string>
#include <vector>

namespace ergo::package_cli {

struct Args {
    std::string              command;
    std::string              host = ".";  ///< ホストのリポ
    std::string              ergo;        ///< Ergo のチェックアウト (空なら既定を探す)
    std::string              code;
    std::string              build;
    std::string              cache;
    std::string              cmake;
    std::vector<std::string> names;
    bool                     upm    = false;
    bool                     json   = false;
    bool                     strict = false;
    bool                     help   = false;
};

/// 失敗したら false と理由 (使い方の誤り)。
bool parse_args(const std::vector<std::string>& argv, Args& out, std::string* error);

const char* usage();

} // namespace ergo::package_cli
