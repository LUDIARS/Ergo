#pragma once

/// ergo_graph_layout — 抽象グラフを層別に配置して `tela_graph::Graph` にする。
///
/// 入力はノード (ID・グループ・札) と有向辺 (使う側 → 使われる側) だけで、
/// 座標は持たない。 配置の性質 (spec/module/graph_layout.md):
///   * 依存の向きは左 (使う側) → 右 (使われる側)。 列は最長経路で決める。
///     循環は ID 順の深さ優先で見つけた戻り辺を逆向きに数えて切る (辺は消さない)。
///   * 列内の順は ID 順から始め、 重心法を 2 往復 (右向き・左向きを 2 回ずつ)
///     かけた後、 同じ重心は ID 順で固定する。
///   * 2 列以上をまたぐ辺は途中の列に通り道 (幅だけの仮の枠) を取り、 ノードを
///     横切らない。 辺は縦横の折れ線で列間の通路を通り、 長さ 0 の区間を作らない。
///   * ノードの高さは札の行数 (" | " 区切り) から決める。 キャンバスは ±100000 内
///     (超えるときは全体を等倍に縮める)。
///   * 決定的 (同じ入力から同じバイト列の図)。 ハッシュ表の走査順に依存しない。
///
/// 自己ループ (from == to) と端点が無い辺は描けないので落とす (`dropped_edges` に数える)。

#include "ergo/tela_graph/graph.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ergo::graph_layout {

struct LayoutGroup {
    std::string       id;
    std::string       name;
    tela_graph::Color color;
    bool              visible = true;
};

struct LayoutNode {
    std::string id;
    std::string group_id;
    /// 行は " | " で区切る (Tela の札は 1 行で、 区切りをそのまま表示する)。
    std::string label;
};

/// 使う側 (from) → 使われる側 (to)。 `dashed` は読み取り・移植など実リンクでない関係。
struct LayoutEdge {
    std::string from;
    std::string to;
    bool        dashed = false;
};

struct LayoutInput {
    std::string              project;
    std::string              title;
    std::vector<LayoutGroup> groups;   ///< この順で出す。 ノードの無いグループは出さない。
    std::vector<LayoutNode>  nodes;
    std::vector<LayoutEdge>  edges;
};

struct LayoutOptions {
    float node_width   = 240.0f;
    float line_height  = 18.0f;
    float node_padding = 10.0f;
    float column_gap   = 96.0f;    ///< 列間の通路の幅
    float row_gap      = 24.0f;
    float margin       = 32.0f;
    float dummy_height = 8.0f;     ///< 途中の列に取る通り道の高さ
};

struct LayoutResult {
    tela_graph::Graph graph;
    std::size_t       dropped_edges = 0;   ///< 自己ループ・端点不明で落とした辺
    std::size_t       columns       = 0;
};

/// 札の行数 (" | " 区切り)。 空の札も 1 行。
std::size_t label_line_count(const std::string& label);

/// 札の行数から決まるノードの高さ。
float node_height_for(const std::string& label, const LayoutOptions& options = {});

/// 配置する。 辺の ID は入力の辺の順に "e0", "e1", … (落とした辺は詰める)。
LayoutResult layout_layered(const LayoutInput& input, const LayoutOptions& options = {});

} // namespace ergo::graph_layout
