#pragma once

/// ergo_tela_graph — 関係図を Tela の `tela::Graph` に変換する (プロセス内で描く経路)。
///
/// `Tela::Graph` ターゲットがあるときだけビルドされる (`ERGO_TELA_GRAPH_HAS_TELA`、
/// C++20)。 ファイルを介さずにホストのオーバーレイへ載せたいときに使う:
///   auto graph = ergo::tela_graph::to_tela_graph(ergo::render_viz::layout_pipeline(snapshot));
///   auto doc   = tela::graph_document(graph, viewport, toggle);
/// 変換は `TELA_GRAPH 1` の書き出しと同じ検査を通し、 通らなければ例外を投げる
/// (Tela 側も不正な図を部分的には使わない)。

#include "ergo/tela_graph/graph.h"

#include <tela/graph.hpp>

namespace ergo::tela_graph {

/// 検査に通らなければ `std::invalid_argument` を投げる。
tela::Graph to_tela_graph(const Graph& graph);

} // namespace ergo::tela_graph
