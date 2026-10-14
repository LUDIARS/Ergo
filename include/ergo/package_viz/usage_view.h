#pragma once

/// ergo_package_viz — P3 使用関係図 (`--view usage`) の抽象グラフ。
///
/// 入力は ergo_package_scan の UsageModel (観測対象のチェックアウトを読んだ結果)。
/// グループ: Ergo / ツール / MUSA / 関連 / 共有 TS / 未観測。 観測対象は直接の辺が無くても
/// 存在を示すために出す (統合設計 N5: 女神名のリポと Ars-Musa は MUSA 1 グループ)。
/// 辺 (使う側 → 使われる側): 実線 = submodule・file: / workspace: / @ludiars/* 依存、
/// 破線 = 移植 (ported_from)・語彙共有 (relations)。 同じ向きに実線と破線が重なれば実線。
/// Tela の辺は札を持たないので、 根拠 (ファイル:行 キー) は使う側のノードの札に載せる。
/// 上限・簡約・焦点・分割は P1 / P2 と同じ手順 (fit.h)。

#include "ergo/package_scan/usage_model.h"
#include "ergo/package_viz/fit.h"
#include "ergo/package_viz/view_graph.h"

#include <cstddef>
#include <string>

namespace ergo::package_viz {

/// 札に載せる根拠の行数の上限 (超えた分は「…他 N 件」)。
inline constexpr std::size_t kUsageEvidenceLines = 3;

/// ノード ID は UsageNode の ID そのもの。
ViewGraph build_usage_view(const package_scan::UsageModel& usage, const ViewOptions& options = {});

/// P3 を作る (build_usage_view → fit_view)。
bool make_usage_view(const package_scan::UsageModel& usage, const ViewOptions& options,
                     FitResult& result, std::string* error = nullptr);

} // namespace ergo::package_viz
