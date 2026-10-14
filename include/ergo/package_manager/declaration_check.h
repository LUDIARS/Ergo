#pragma once

// ホストの宣言の検査 (構成時の PKG 規則のうち宣言だけで決まるものを CLI から当てる)。
//
//   PKG-01  use / optional / features が知らないパッケージを名指し
//   PKG-02  host.ergo.rev と非公開 link パッケージの rev が 40 桁の小文字 SHA でない
//   PKG-15  非公開パッケージが Ergo のマニフェストの名前 (packages / modules / observe) と同じ
//   MANIFEST  host.ergo が無い (warning)、 optional が use に無い (warning)、 features の書式、
//             legacy_vars の行き先、 非公開パッケージの kind / repository
//
// Ergo のマニフェストが無い (nullptr) ときは、 それと照合する規則 (PKG-01 の公開名、 PKG-15) を飛ばす。

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/finding.h"
#include "ergo/package_manager/host_declaration.h"

#include <vector>

namespace ergo::package_manager {

/// 40 桁の小文字 16 進か (PKG-02)。
bool is_full_sha(const std::string& rev);

/// 結果は sort_findings 済み。
std::vector<Finding> check_declaration(const HostDeclaration& host, const ErgoManifest* ergo);

} // namespace ergo::package_manager
