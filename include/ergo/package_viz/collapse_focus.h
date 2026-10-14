#pragma once

/// ergo_package_viz — 焦点と畳み込み (上限に収める手順の 3 段目)。
///
/// 畳み込み (targets 図): 展開するパッケージ (host・Ergo・`--expand`) 以外のターゲットを
/// パッケージごとに 1 ノード (`pkg:<名前>`) にまとめる。 辺は付け替え、 自己ループは落とし、
/// 重なる辺は 1 本にする (実線が勝つ)。
/// 焦点: パッケージ名 / 層名 / ターゲット名 / ノード ID のどれかに当たるノードと、
/// その 1 歩の隣 (向きを問わない) だけを残す。

#include "ergo/package_model/package_model.h"
#include "ergo/package_viz/view_graph.h"

#include <set>
#include <string>
#include <vector>

namespace ergo::package_viz {

/// 既定で展開するパッケージ (host と Ergo) に `expand` を足したもの。
std::set<std::string> expanded_packages(const package_model::PackageModel& model,
                                        const std::vector<std::string>& expand);

/// 畳んだパッケージ名を返す (畳むものが無ければ空)。
std::vector<std::string> collapse_packages(ViewGraph& view,
                                           const package_model::PackageModel& model,
                                           const std::set<std::string>& expanded);

/// 当たったノードが 1 つも無ければ false (グラフは変えない)。 省いたノード数を `dropped` に。
bool apply_focus(ViewGraph& view, const std::vector<std::string>& focus,
                 std::size_t* dropped = nullptr);

} // namespace ergo::package_viz
