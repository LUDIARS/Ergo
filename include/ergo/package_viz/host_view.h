#pragma once

/// ergo_package_viz — P4 ホスト状態図 (`--view host`、 spec/feature/package-manager.md §5.1)。
///
/// P1 / P2 が構成の形を見る図なのに対し、 P4 は 1 ホストの今の状態を見る図。 入力は
/// package_model::HostState (resolved.json + watch-report.json + status.json。 どれが欠けても描き、 札は "—")。
///
/// グループ: host / Ergo / 依存 / 更新: タグ・main / 更新: 審査中・作業中 / 検査 (6)。
/// 辺: host → Ergo・依存 (実線)、 依存 → 更新 (破線)、 依存 → 検査 (破線。 Tela の辺は実線と破線しか無いので
/// §5.1 の点線は破線で描き、 検査のグループの色で見分ける)。
/// 審査中・作業中 (review / run) が kHostReviewNodes を超えたら由来 (Pf / AI / 人 / 不明) ごとに 1 ノードへ畳み、
/// 件数を札に出す。 上限・焦点・分割は P1〜P3 と同じ手順 (fit.h)。 同じ入力なら同じ図 (決定的)。

#include "ergo/package_model/host_state.h"
#include "ergo/package_viz/fit.h"
#include "ergo/package_viz/view_graph.h"

#include <cstddef>
#include <string>

namespace ergo::package_viz {

/// 審査中・作業中を 1 件 1 ノードで描く上限 (超えたら由来ごとに畳む)。
inline constexpr std::size_t kHostReviewNodes = 12;

/// 札で欠けた値を表す字。
inline constexpr const char* kHostMissing = "\xE2\x80\x94";   // "—"

/// ノード ID: "host"、 "ergo"、 依存は "dep:<名前>"、 更新は "update:<依存>"、
/// 審査中・作業中は "review:<依存>:<主体>" (畳んだら "review:origin:<由来>")、 検査は "check:<ID>:<対象>"。
ViewGraph build_host_view(const package_model::HostState& state, const ViewOptions& options = {});

/// P4 を作る (build_host_view → fit_view)。
bool make_host_view(const package_model::HostState& state, const ViewOptions& options,
                    FitResult& result, std::string* error = nullptr);

} // namespace ergo::package_viz
