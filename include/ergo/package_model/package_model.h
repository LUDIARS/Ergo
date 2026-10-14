#pragma once

/// ergo_package_model — パッケージ構成の値型 (読むだけ)。
///
/// 入力は構成時にハブが書く `<build>/ergo-packages.resolved.json` と、 CMake File API
/// codemodel v2 の返答 (`<build>/.cmake/api/v1/reply/`)。 図 (ergo_package_viz) と
/// 宣言漏れの検査 (PKG-17) はこの型だけを見る。 どちらのファイルも書き換えない。
/// 仕様: spec/module/package_model.md、 spec/feature/package-hub.md §5。

#include <string>
#include <vector>

namespace ergo::package_model {

/// パッケージの種類。 resolved.json の `kind` (link / tool) に、 File API から
/// 見つけた host・Ergo・第三者と、 マニフェストの observe を足したもの。
enum class PackageKind { Host, Ergo, Link, Tool, ThirdParty, Observe };

const char* to_string(PackageKind kind);

/// 図の札に載せる版。 出所 (`revisor` / `git` / `cmake`) を必ず添える。
struct VersionLabel {
    std::string text;     ///< 例: "2.2.0+12"。 空なら版不明
    std::string source;   ///< "revisor" / "git" / "cmake" / ""
};

struct Package {
    std::string              name;
    std::string              code;
    PackageKind              kind = PackageKind::Host;
    std::string              group;        ///< マニフェストの group (link / tool / observe)
    std::string              state;        ///< absent / host-unmanaged / hub-managed / …
    std::string              pin;          ///< 40 桁 SHA (link)
    std::string              head;         ///< 解決した checkout の HEAD
    std::string              drift;        ///< none / ahead / behind / diverged / unknown
    std::string              route;
    std::string              mode;
    std::string              rev_override;
    std::string              expect_tag;
    std::string              source_dir;   ///< 帰属に使う絶対パス (無ければ "")
    std::string              cmake_version;///< project(VERSION)
    std::string              path;         ///< tool: リポ相対のパス
    std::string              gitlink;      ///< tool: gitlink SHA
    bool                     dirty       = false;
    bool                     initialized = false;   ///< tool: submodule が取得済み
    std::vector<std::string> features;
    std::vector<std::string> requires_;    ///< 宣言した依存パッケージ (`requires` のキー)
    std::vector<std::string> targets_present;
    VersionLabel             version;
};

struct Module {
    std::string              name;
    bool                     enabled = false;
    std::vector<std::string> needs;
    std::vector<std::string> optional;
    std::vector<std::string> ported_from;
    std::string              reason;
};

struct Relation {
    std::string from;   ///< 例: "ergo_resource"
    std::string kind;   ///< 例: "shares-vocabulary"
};

struct ObserveEntry {
    std::string           name;
    std::string           code;
    std::string           group;       ///< musa / related
    std::string           path_hint;
    std::vector<Relation> relations;
};

struct Finding {
    std::string id;         ///< 例: "PKG-05"
    std::string severity;   ///< error / warning / record
    std::string subject;
    std::string message;
};

/// File API のターゲット 1 つ。 `package` は帰属したパッケージ名。
struct Target {
    std::string              id;            ///< File API の id (一意)
    std::string              name;
    std::string              type;          ///< STATIC_LIBRARY / EXECUTABLE / …
    std::string              package;
    std::string              layer;         ///< .anatomia/layers.json の層 ("" = 未分類)
    std::string              source_dir;    ///< 絶対パス ('/' 区切り)
    std::string              project;       ///< 属する CMake project 名
    std::vector<std::string> sources;       ///< ソースのパス (トップのソース相対か絶対)
    std::vector<std::string> dependencies;  ///< 直接の依存 (ターゲット id)
    bool                     excluded = false;   ///< 既定で除外 (テスト・ダミー・ベンチ・gtest_main)
};

struct PackageModel {
    // resolved.json
    std::string               mode;
    bool                      strict = false;
    std::string               platform;
    std::string               manifest;     ///< マニフェストのパス (Ergo のソース相対のことがある)
    std::string               host_id;      ///< `host.id` (ホストのコード。 宣言が無ければ project 名)
    std::vector<Package>      packages;     ///< 名前順
    std::vector<Module>       modules;      ///< 名前順
    std::vector<ObserveEntry> observe;      ///< 名前順 (packages にも Observe として載る)
    std::vector<Finding>      findings;

    // File API (has_codemodel が false なら空)
    bool                      has_codemodel = false;
    std::string               source_root;  ///< トップのソース ('/' 区切り)
    std::string               build_root;
    std::string               configuration;
    std::string               ergo_root;    ///< project(Ergo) のソース
    std::vector<Target>       targets;      ///< 名前順

    const Package* find_package(const std::string& name) const;
    const Target*  find_target_by_id(const std::string& id) const;
    const Module*  find_module(const std::string& name) const;
};

} // namespace ergo::package_model
