#pragma once

/// ergo_package_viz — 推移簡約。 実線 u → v のうち、 その辺を使わずに u から v へ
/// 実線でたどれるものを消す (到達関係は変わらない)。 破線は残す。
/// 辺は (from, to) の順に 1 本ずつ判定し、 消した後のグラフで次を判定する
/// (循環があっても決定的)。 同じ向きの重複した実線は 1 本にまとめ、 消した本数に数える。

#include "ergo/graph_layout/layered_layout.h"

#include <cstddef>

namespace ergo::package_viz {

/// 消した本数を返す。
std::size_t transitive_reduction(graph_layout::LayoutInput& graph);

} // namespace ergo::package_viz
