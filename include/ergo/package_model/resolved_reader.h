#pragma once

/// ergo_package_model — `ergo-packages.resolved.json` (schema 1) を読む。
///
/// 書式は spec/data/ergo-packages.md の「resolved.json」。 `schema` が 1 でなければ
/// 明示エラー (読み違えた図を出さない)。 link / tool はそのまま、 observe は
/// `PackageKind::Observe` のパッケージとしても載せる。 `version` (project(VERSION)) は
/// 出所 "cmake" の版の札にする。

#include "ergo/package_model/package_model.h"

#include <string>

namespace ergo::package_model {

/// JSON テキストから。 失敗したら false と理由。 `model` の resolved 部分を置き換える。
bool read_resolved_text(const std::string& json, PackageModel& model, std::string* error = nullptr);

/// ファイルから。
bool read_resolved_file(const std::string& path, PackageModel& model, std::string* error = nullptr);

} // namespace ergo::package_model
