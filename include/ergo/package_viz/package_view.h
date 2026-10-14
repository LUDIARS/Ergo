#pragma once

/// ergo_package_viz — P1 パッケージ版一覧 (`--view packages`) の抽象グラフ。
///
/// ノード: パッケージ (host・Ergo・link・tool・第三者・observe)。 札は名前 (コード) /
/// 版 (出所付き) / SHA7 / 状態 / 機能 / 印 (★drift dirty 上書き 宣言漏れ 未観測)。
/// 辺 (使う側 → 使われる側):
///   * 実線: link の `requires`、 File API の実リンクをパッケージ単位に畳んだもの
///     (既定で除外したターゲットは数えない)。
///   * 破線: モジュールの `ported_from` (Ergo → 移植元)、 observe の `relations`
///     (関係の from がモジュールなら Ergo から)。
/// 同じ向きの実線と破線が重なれば実線を残す。

#include "ergo/package_model/package_model.h"
#include "ergo/package_viz/view_graph.h"

namespace ergo::package_viz {

/// ノード ID はパッケージ名 ("pkg:" を付けない)。
ViewGraph build_packages_view(const package_model::PackageModel& model,
                              const ViewOptions& options = {});

} // namespace ergo::package_viz
