#pragma once

/// ergo_tela_graph — 関係図を Tela の `TELA_GRAPH 1` ファイルとして書き出す。
///
/// 書き出したファイルは Tela の既存ホストでそのまま開ける:
///   tela_view    --font <file.ttf> --graph <file>            (自前のウインドウ)
///   tela_overlay --font <file.ttf> --graph <file> --attach … (作業中のウインドウへ重ねる)
///
/// 形式と上限は Tela `spec/contracts.md` SPEC-TL-GRAPH に従う。 Tela は上限超過や
/// 不整合のあるファイルを部分的に使わず全体を拒否するので、 ここでも書く前に
/// 同じ条件で検査し、 満たさなければ何も書かない。

#include "ergo/tela_graph/graph.h"

#include <cstddef>
#include <string>

namespace ergo::tela_graph {

/// Tela 側の上限 (SPEC-TL-GRAPH)。
inline constexpr std::size_t kTelaMaxGroups   = 8;
inline constexpr std::size_t kTelaMaxNodes    = 128;
inline constexpr std::size_t kTelaMaxEdges    = 256;
inline constexpr std::size_t kTelaMaxPoints   = 4096;
inline constexpr std::size_t kTelaMaxIdBytes  = 256;
inline constexpr std::size_t kTelaMaxBytes    = 4u * 1024u * 1024u;
inline constexpr float       kTelaMaxCoord    = 100000.0f;

/// 図が Tela の受け付ける条件を満たすか。 満たさなければ `error` に最初の理由を入れる。
bool validate_tela_graph(const Graph& graph, std::string* error = nullptr);

/// `TELA_GRAPH 1` のテキストにする (LF 改行・BOM なし)。 検査に通らなければ
/// 空文字列を返し、 `error` に理由を入れる。 改行などの制御文字はフィールド内で
/// 空白に置き換える (1 レコード 1 行の形式を壊さないため)。
std::string to_tela_graph_text(const Graph& graph, std::string* error = nullptr);

/// ファイルへ書く。 無い親フォルダは作る。 検査や I/O に失敗したら false (既存ファイルには触れない)。
bool save_tela_graph(const std::string& path, const Graph& graph,
                     std::string* error = nullptr);

} // namespace ergo::tela_graph
