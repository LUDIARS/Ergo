#pragma once

// ergo_package_manager の内部部品: add / remove の共通処理 (宣言の読み直しと、 書き換えで新しく出た error の検出)。

#include "ergo/package_manager/declaration_edit.h"
#include "ergo/package_manager/host_declaration.h"

#include <string>

namespace ergo::package_manager::detail {

EditResult edit_error(const std::string& why);

/// host_text をホストの宣言として読む。 読めなければ false と理由。
bool parse_host_text(const std::string& host_text, HostDeclaration& out, std::string& error);

/// 書き換え前後を検査し、 後にだけある error があれば拒んだ結果、 無ければ text を持つ ok の結果。
EditResult finish_edit(const std::string& before, std::string after, const ErgoManifest* ergo);

bool contains(const std::vector<std::string>& v, const std::string& x);

} // namespace ergo::package_manager::detail
