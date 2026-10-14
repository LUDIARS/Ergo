#pragma once

// ergo_package_viz CLI の引数。 解析だけを持つ (実行は main.cpp)。
//
//   ergo_package_viz <build-dir> --view packages|targets [--focus X]... [--expand P]...
//                    [--include-tests] [--config NAME] [--layers FILE] [--resolved FILE]
//                    [--no-version-probe] --out FILE.tela
//   ergo_package_viz <build-dir> --check [--include-tests] [--config NAME]
//   ergo_package_viz <build-dir> --view usage [--root DIR] [--manifest FILE] [--focus X]... --out FILE.tela
//   ergo_package_viz <build-dir> --scan [--root DIR] [--manifest FILE]

#include "ergo/package_viz/view_graph.h"

#include <string>
#include <vector>

namespace ergo::package_viz::cli {

struct Options {
    std::string build_dir;
    std::string out;
    std::string config;
    std::string layers;
    std::string resolved;
    std::string root;       ///< usage / scan: 兄弟チェックアウトの親
    std::string manifest;   ///< usage / scan: ergo-packages.json (空なら resolved.json から)
    bool        check         = false;
    bool        scan          = false;   ///< 観測の辺を根拠付きで表示するだけ
    bool        version_probe = true;
    bool        help          = false;
    ViewOptions view;
};

/// 失敗したら false と理由 (使い方の誤り)。
bool parse_options(const std::vector<std::string>& args, Options& out, std::string* error);

const char* usage();

} // namespace ergo::package_viz::cli
