#pragma once

// ergo_package_viz の内部部品: ノードの札 (" | " で行を区切る)。
// 札の行数がノードの高さになる (ergo_graph_layout)。

#include "ergo/package_model/package_model.h"

#include <cstddef>
#include <set>
#include <string>

namespace ergo::package_viz::detail {

/// P1 のパッケージ札: 名前 (コード) / 版 (出所付き) / SHA7 / 状態 / 機能 / 印。
/// `undeclared` はこのパッケージから出る宣言漏れがあるか。
std::string package_label(const package_model::Package& p, bool undeclared,
                          std::size_t target_count, const package_model::PackageModel& model);

/// 印の行 ("★drift:ahead dirty 上書き 宣言漏れ 未観測")。 無ければ ""。
std::string marks_line(const package_model::Package& p, bool undeclared);

/// P2 のターゲット札: 名前 / 型 / パッケージ · 層 / 印。
std::string target_label(const package_model::Target& t, bool undeclared);

/// P2 で畳んだパッケージの札。
std::string collapsed_label(const package_model::Package& p, std::size_t target_count);

} // namespace ergo::package_viz::detail
