#pragma once

/// ergo_package_scan — マニフェスト (`ergo-packages.json`) のうち観測に要る所だけを読む。
///
/// 読むのは `observe` (観測対象と `relations`)、 `modules.<m>.ported_from`、 tool パッケージの
/// `path` (Lapilli など。 依存先の帰属に使う)。 根拠に載せるため値ごとの行番号を持つ。
/// link パッケージの検査や取り込みは CMake 側 (cmake/packages/) の責務で、 ここでは見ない。

#include <string>
#include <vector>

namespace ergo::package_scan {

struct ManifestRelation {
    std::string from;   ///< 例: "ergo_resource"
    std::string kind;   ///< 例: "shares-vocabulary"
    int         line = 0;
    std::string key;    ///< 例: "observe.curare.relations[0]"
};

struct ManifestObserve {
    std::string                   name;
    std::string                   code;
    std::string                   group;
    std::string                   path_hint;
    std::vector<ManifestRelation> relations;
};

struct ManifestPort {
    std::string module;    ///< 例: "ergo_resource"
    std::string package;   ///< 例: "clio"
    int         line = 0;
    std::string key;       ///< 例: "modules.ergo_resource.ported_from[0]"
};

struct ManifestTool {
    std::string name;
    std::string code;
    std::string path;   ///< Ergo のソースからの相対 (例: "lib/lapilli")
};

struct ScanManifest {
    std::vector<ManifestObserve> observe;   ///< 名前順
    std::vector<ManifestPort>    ports;     ///< (モジュール, 添字) 順
    std::vector<ManifestTool>    tools;     ///< 名前順
};

/// `schema` が 1 でなければ false。
bool parse_scan_manifest(const std::string& text, ScanManifest& out, std::string* error = nullptr);

} // namespace ergo::package_scan
