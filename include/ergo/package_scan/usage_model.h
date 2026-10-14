#pragma once

/// ergo_package_scan — 観測対象の使用関係 (P3 usage の入力) の値型。
///
/// `scan_usage` (scan.h) が兄弟チェックアウトの `package.json` / `.gitmodules` / gitlink と、
/// Ergo のマニフェストの `ported_from` / `relations` を読んで作る。 辺には必ず根拠
/// (ファイル・行・キー) を 1 つ以上持たせ、 根拠の無い辺は作らない。
/// 仕様: spec/module/package_scan.md、 spec/feature/package-hub.md §5。

#include <string>
#include <vector>

namespace ergo::package_scan {

/// ノードの種類 (P3 のグループに対応する)。
enum class UsageKind {
    Musa,        ///< observe の group が musa
    Related,     ///< observe の group が related (など musa 以外)
    SharedTs,    ///< 依存先として現れた共有ライブラリ (Lapilli・@ludiars/* など)
    Ergo,        ///< Ergo 本体と、 移植・語彙共有の根拠を持つ Ergo のモジュール
    Tool,        ///< Ergo の Web ツール (tools/ergo など)
    Unobserved,  ///< observe にあるが読めなかったもの
};

const char* to_string(UsageKind kind);

/// 辺の根拠。 `file` は読んだ根 (兄弟の root か Ergo のソース) からの相対パス ('/' 区切り)。
struct Evidence {
    std::string file;   ///< 例: "Thaleia/.gitmodules"
    int         line = 0;   ///< 1 始まり。 0 は行が分からない
    std::string key;    ///< 例: "submodule.lib/lapilli.url" / "dependencies.@ludiars/one-shot"
    std::string note;   ///< 例: "gitlink 1234567" / "file:lib/lapilli/packages/one-shot"
};

/// 辺の種類。 実線 = submodule / file: / workspace: / @ludiars/*、 破線 = 移植 / 語彙共有。
enum class UsageEdgeKind { Submodule, FileDep, WorkspaceDep, ScopedDep, PortOf, SharesVocabulary };

const char* to_string(UsageEdgeKind kind);
bool        is_dashed(UsageEdgeKind kind);

struct UsageNode {
    std::string id;          ///< observe 名 / モジュール名 / "ergo" / "tools/ergo" / "npm:<名>" / "repo:<名>"
    std::string name;        ///< 札の 1 行目
    std::string code;        ///< プロジェクトコード (分かれば)
    UsageKind   kind = UsageKind::SharedTs;
    std::string declared_group;   ///< observe の group (未観測でも元の group を残す)
    std::string reason;      ///< 未観測の理由
    std::string package_name;     ///< 読んだ package.json の name (あれば)
};

struct UsageEdge {
    std::string                from;   ///< 使う側
    std::string                to;     ///< 使われる側
    std::vector<UsageEdgeKind> kinds;  ///< 重なった種類 (順序は種類の宣言順)
    std::vector<Evidence>      evidence;   ///< 1 つ以上
    bool dashed() const;               ///< 実線の種類が 1 つも無ければ破線
};

struct UsageModel {
    std::vector<UsageNode>   nodes;        ///< ID 順
    std::vector<UsageEdge>   edges;        ///< (from, to) 順
    std::vector<std::string> files_read;   ///< 実際に開いたファイル (根からの相対、 名前順)
    std::vector<std::string> notes;        ///< 読み飛ばした入力など

    const UsageNode* find_node(const std::string& id) const;
    const UsageEdge* find_edge(const std::string& from, const std::string& to) const;
};

} // namespace ergo::package_scan
