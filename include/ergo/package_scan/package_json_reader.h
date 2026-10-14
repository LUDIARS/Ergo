#pragma once

/// ergo_package_scan — `package.json` から使用関係に効く依存だけを取り出す。
///
/// 読むのは `name` と `dependencies` / `devDependencies` のうち、 指定が `file:` /
/// `workspace:` か、 名前が `@ludiars/` で始まるものだけ。 それ以外 (npm の公開パッケージ)
/// は使用関係の根拠にならないので返さない。 行番号は JsonLineIndex で付ける。

#include <string>
#include <vector>

namespace ergo::package_scan {

enum class DepSpecKind { File, Workspace, Scoped };

struct PackageDep {
    std::string section;   ///< "dependencies" / "devDependencies"
    std::string name;      ///< 例: "@ludiars/one-shot"
    std::string spec;      ///< 例: "file:lib/lapilli/packages/one-shot"
    DepSpecKind kind = DepSpecKind::Scoped;
    int         line = 0;
};

struct PackageJson {
    std::string             name;
    int                     name_line = 0;
    std::vector<PackageDep> deps;   ///< section 順 → 名前順
};

/// 壊れた JSON は false。
bool parse_package_json(const std::string& text, PackageJson& out, std::string* error = nullptr);

/// `file:` の後ろのパス (それ以外の指定なら "")。
std::string file_spec_path(const std::string& spec);

} // namespace ergo::package_scan
