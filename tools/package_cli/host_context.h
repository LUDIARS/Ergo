#pragma once

// ergo_pkg の各コマンドが共通で読むもの: ホストの宣言、 Ergo のチェックアウト (マニフェストと
// ブートストラップのひな形)。 どれが欠けても作れ、 欠けたことは notes と has_* で分かる。

#include "cli_args.h"

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/finding.h"
#include "ergo/package_manager/host_declaration.h"

#include <optional>
#include <string>
#include <vector>

namespace ergo::package_cli {

namespace pm = ergo::package_manager;

struct HostContext {
    std::string host_dir;

    std::optional<std::string>   declaration_text;  ///< <host>/ergo-packages.json
    bool                         has_declaration = false;  ///< 読めて `host` 節がある
    pm::HostDeclaration          declaration;
    std::vector<pm::Finding>     read_findings;     ///< 宣言の形の不備

    std::string                  ergo_dir;          ///< 見つからなければ ""
    bool                         has_ergo_manifest = false;
    pm::ErgoManifest             ergo_manifest;
    std::optional<std::string>   bootstrap_template;

    std::vector<std::string>     notes;             ///< 欠けたものの説明
};

/// Ergo のチェックアウト: --ergo → 環境変数 ERGO_SOURCE_DIR → このツールを作った Ergo。
std::string locate_ergo(const Args& args);

/// 宣言が JSON として壊れていれば false と理由 (引数・入力の誤り)。 無いだけなら true。
bool load_host_context(const Args& args, HostContext& out, std::string* error);

} // namespace ergo::package_cli
