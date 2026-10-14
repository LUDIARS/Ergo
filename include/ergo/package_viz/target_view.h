#pragma once

/// ergo_package_viz — P2 ターゲットのリンク図 (`--view targets`) の抽象グラフ。
///
/// ノード: File API のターゲット (既定で除外したものは除く)。 辺: 直接の依存 (実線)。
/// グループ: Ergo のターゲットは `.anatomia/layers.json` の層 (当たらなければ shared)、
/// host は層が無ければ application、 link / tool は外部パッケージ、 第三者は第三者。
/// 依存の循環 (強連結成分が 2 以上) に入るターゲットは `cycle` グループへ移して見せる
/// (構成は止めない)。 宣言漏れ (PKG-17) の元になるターゲットは札に印。
/// 畳み込み (他パッケージを 1 ノードに) はここではせず、 上限に収める手順 (`fit.h`) が行う。

#include "ergo/package_model/package_model.h"
#include "ergo/package_viz/view_graph.h"

#include <string>
#include <vector>

namespace ergo::package_viz {

ViewGraph build_targets_view(const package_model::PackageModel& model,
                             const ViewOptions& options = {});

/// 有向グラフの強連結成分のうち 2 ノード以上のもの (ノード添字、 成分内は添字順、
/// 成分は最小添字順)。 `adjacency[i]` は i の出辺の行き先。
std::vector<std::vector<std::size_t>> cyclic_components(
    const std::vector<std::vector<std::size_t>>& adjacency);

} // namespace ergo::package_viz
