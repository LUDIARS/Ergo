#pragma once

/// ergo_tela_graph — 配置済みの関係図。
///
/// Tela の `TELA_GRAPH 1` (Tela `spec/contracts.md` SPEC-TL-GRAPH) と同じ形の値型。
/// Tela はレイアウトを計算しないので、 ノード位置と辺の経路は図を作る側
/// (例: ergo_render_viz の layout_pipeline) が確定させる。 Ergo の可視化
/// (描画パイプライン・パッケージ構成など) はこの型を共通の出口にする。
/// Tela への依存は持たない (書き出しは `writer.h`、 Tela の型への変換は
/// `tela_adapter.h`)。

#include <cstdint>
#include <string>
#include <vector>

namespace ergo::tela_graph {

struct Color {
    std::uint8_t r = 0, g = 0, b = 0;
};

struct Point {
    float x = 0.0f, y = 0.0f;
};

/// 分類 (Tela 上で表示を切り替えられる単位)。 図の列順で並べる。
struct Group {
    std::string id;
    std::string name;
    bool        visible = true;
    Color    color;
};

struct Node {
    std::string group_id;
    std::string id;
    std::string label;
    float       x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
};

/// 有向の関係。 Tela は終端に矢じりを描く。 `dashed` は読み取り (入力) を表す。
struct Edge {
    std::string           id;
    std::string           from;
    std::string           to;
    bool                  dashed = false;
    std::vector<Point> points;   ///< 経路順。 2 点以上。
};

struct Graph {
    std::string           project;
    std::string           title;
    float                 width  = 0.0f;
    float                 height = 0.0f;
    std::vector<Group> groups;
    std::vector<Node>  nodes;
    std::vector<Edge>  edges;
};

} // namespace ergo::tela_graph
