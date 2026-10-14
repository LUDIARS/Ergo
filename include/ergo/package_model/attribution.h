#pragma once

/// ergo_package_model — File API のターゲットをパッケージへ帰属させる。
///
/// 規則 (spec/module/package_model.md):
///   1. ターゲットを定義したディレクトリ (`source_dir`) が最も長く一致する根へ帰属する。
///      根は link パッケージの `source_dir` (resolved.json) と Ergo (`project(Ergo)`) と
///      host (トップのソース。 Ergo がトップなら無い)。
///   2. どの根にも入らず、 project 名 (小文字) が既知のパッケージ名なら、 そのパッケージ。
///   3. トップのソースの外にあれば第三者 (project 名を小文字にした名前のパッケージを足す)。
///   4. それ以外は host。
/// ergo / host / 第三者のパッケージは見つかったものだけ `model.packages` に足す
/// (ergo は File API が無くても足す)。
///
/// 既定の除外 (図と PKG-17 から外す): テスト (`test_` で始まる名前、 またはソースが
/// すべて tests/ の下)、 ダミー (`_dummy` を含む)、 ベンチ (`bench_` で始まる、 または
/// benchmarks/ の下)、 `ergo_gtest_main`。

#include "ergo/package_model/package_model.h"

namespace ergo::package_model {

/// 既定で除外するターゲットか。 `rel_sources` はパッケージの根からの相対パス。
bool excluded_by_default(const std::string& name, const std::vector<std::string>& rel_sources);

/// 帰属と既定の除外を `model.targets` に入れ、 ergo / host / 第三者を `model.packages` に足す。
void attribute_targets(PackageModel& model);

} // namespace ergo::package_model
