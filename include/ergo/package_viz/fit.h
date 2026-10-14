#pragma once

/// ergo_package_viz — 図を作る入口。 上限 (8 グループ / 128 ノード / 256 辺) に収める手順を
/// 決まった順にかけ、 収まった所で止める (spec/feature/package-hub.md §5):
///   1. 既定除外 (組み立て時。 `include_tests` で戻す)
///   2. 推移簡約 (消した本数を題に添える)
///   3. 焦点と畳み込み (targets 図は host と Ergo を展開・他は 1 ノード。 `--focus` /
///      `--expand` を指定したときは収まっていてもかける)
///   4. 分割 (`out.N.tela` と索引図 `out.index.tela`)
/// どの段でも、 配置した図が `validate_tela_graph` を通らない 1 枚は返さない
/// (経路の点数の上限などで落ちた枚は、 1 枚あたりのノード数を半分にして分け直す)。

#include "ergo/package_model/package_model.h"
#include "ergo/package_viz/view_graph.h"
#include "ergo/tela_graph/graph.h"

#include <string>
#include <utility>
#include <vector>

namespace ergo::package_viz {

struct FitResult {
    std::vector<tela_graph::Graph> graphs;   ///< 1 枚なら分割なし
    bool                           split = false;
    tela_graph::Graph              index;    ///< split のときだけ
    std::vector<std::string>       steps;    ///< かけた手順 (表示用)
};

/// 図を作る (P1 / P2。 P3 は usage_view.h の make_usage_view、 P4 は host_view.h の make_host_view)。
/// 焦点が何にも当たらない・分けても上限に収まらないときは false と理由。
bool make_view(const package_model::PackageModel& model, const ViewOptions& options,
               FitResult& result, std::string* error = nullptr);

/// 組み立て済みの抽象グラフに手順 2〜4 をかけて配置する (make_view の後半。 テスト用に公開)。
bool fit_view(ViewGraph view, const package_model::PackageModel& model,
              const ViewOptions& options, FitResult& result, std::string* error = nullptr);

/// 書き出すファイル名。 分割なしなら `out` だけ、 分割なら `<stem>.1.tela` … と
/// `<stem>.index.tela` (`<stem>` は `out` から末尾の ".tela" を除いたもの)。
std::vector<std::pair<std::string, const tela_graph::Graph*>> output_files(
    const FitResult& result, const std::string& out);

} // namespace ergo::package_viz
