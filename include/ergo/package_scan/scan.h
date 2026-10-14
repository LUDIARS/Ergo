#pragma once

/// ergo_package_scan — 観測対象のチェックアウトを読んで使用関係 (UsageModel) を作る入口。
///
/// 読む範囲は宣言されたものだけ (総なめにしない):
///   * 観測対象: `<root>/<observe.path_hint>` の `package.json`、 `.gitmodules`、
///     `.git/index` (`.git` がそのチェックアウトの中のディレクトリのときだけ、 gitlink の SHA)。
///   * Ergo: マニフェストのあるディレクトリ (`ergo_root`) の `.gitmodules` と `.git/index`、
///     `ergo_paths` (既定 `tools/ergo`) の `package.json`、 マニフェストそのもの。
/// `path_hint` が範囲外 (絶対・".."・':') のもの、 `root` 未指定、 チェックアウトが無いものは
/// 「未観測」ノードとして理由付きで残す。 辺は根拠 (ファイル・行・キー) のあるものだけ。
/// 情報源は兄弟チェックアウト (統合設計 N8)。 Concordia / Revisor への問い合わせはしない。

#include "ergo/package_scan/usage_model.h"

#include <string>
#include <vector>

namespace ergo::package_scan {

/// Ergo の中で package.json を読む場所の既定 (Ergo のソースからの相対)。
std::vector<std::string> default_ergo_paths();

struct ScanOptions {
    std::string              manifest_path;   ///< ergo-packages.json (必須)
    std::string              ergo_root;       ///< 空ならマニフェストのあるディレクトリ
    std::string              root;            ///< 兄弟チェックアウトの親。 空なら観測対象はすべて未観測
    std::vector<std::string> ergo_paths = default_ergo_paths();
};

/// マニフェストが読めなければ false。 観測対象が読めないのは失敗ではない (未観測に残す)。
bool scan_usage(const ScanOptions& options, UsageModel& model, std::string* error = nullptr);

/// 根拠の 1 行表記 ("Thaleia/.gitmodules:3 submodule.lib/lapilli.url (gitlink 1234567)")。
std::string describe(const Evidence& evidence);

} // namespace ergo::package_scan
