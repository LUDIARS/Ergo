#pragma once

/// ergo_package_viz — 図の手前の抽象グラフ (座標なし) と、 図ごとの指定。
///
/// P1 (packages) / P2 (targets) / P3 (usage) / P4 (host) の組み立て (`package_view.h` /
/// `target_view.h` / `usage_view.h` / `host_view.h`) が
/// ViewGraph を作り、 上限に収める手順 (`fit.h`) が簡約・畳み込み・焦点・分割を
/// かけてから ergo_graph_layout で配置する。

#include "ergo/graph_layout/layered_layout.h"
#include "ergo/tela_graph/writer.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace ergo::package_viz {

enum class View { Packages, Targets, Usage, Host };

/// 1 枚の上限 (Tela SPEC-TL-GRAPH と同じ既定値)。
struct Limits {
    std::size_t groups = tela_graph::kTelaMaxGroups;
    std::size_t nodes  = tela_graph::kTelaMaxNodes;
    std::size_t edges  = tela_graph::kTelaMaxEdges;
};

struct ViewOptions {
    View                     view = View::Packages;
    /// 焦点: パッケージ名 / 層名 / ターゲット名 (packages 図ではパッケージ名)。
    /// 当たったノードと 1 歩の隣だけを残す。
    std::vector<std::string> focus;
    /// targets 図で畳まずに展開するパッケージ (host と ergo は常に展開)。
    std::vector<std::string> expand;
    /// 既定の除外 (テスト・ダミー・ベンチ・ergo_gtest_main) を戻す。
    bool                     include_tests = false;
    Limits                   limits;
    graph_layout::LayoutOptions layout;
};

/// ノードの由来 (焦点・畳み込みが見る)。
struct NodeInfo {
    std::string package;
    std::string layer;    ///< targets 図の層 ("" = 層なし)
    std::string target;   ///< targets 図のターゲット名 (畳んだノードは "")
};

struct ViewGraph {
    graph_layout::LayoutInput       graph;
    std::map<std::string, NodeInfo> info;    ///< ノード ID → 由来
    std::vector<std::string>        notes;   ///< 題に添える注記 (簡約した本数など)
};

/// 上限に収まっているか (グループはノードの使うものだけ数える)。
bool fits(const graph_layout::LayoutInput& graph, const Limits& limits);

} // namespace ergo::package_viz
