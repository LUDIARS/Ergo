#pragma once

// ergo_package_viz の内部部品: P4 ホスト状態図の札 (" | " で行を区切る)。 欠けた値は "—"。

#include "ergo/package_model/host_state.h"

#include <string>
#include <vector>

namespace ergo::package_viz::detail {

/// ホストのコード: resolved.json の host.id → status.json → 報告 → "—"。
std::string host_code(const package_model::HostState& state);

/// host: コード / HEAD SHA7 / モード / テスト (`12/12`・`3 失敗`・`未実行`)。
std::string host_label(const package_model::HostState& state);

/// Ergo: 版 / pin SHA7 / 遅れ (`main +18`、 `新タグ v0.10.0`)。 `ergo` は報告の依存 "ergo" (無ければ nullptr)。
std::string ergo_label(const package_model::HostState& state, const package_model::WatchDependency* ergo);

/// 依存: 名前 (コード) / 版 · pin / 経路 · drift / 機能 / 印 (`★drift` `dirty` `off-main`)。
/// `package` は resolved.json の依存、 `watched` は報告の依存 (どちらかは nullptr でよい)。
std::string dependency_label(const std::string& name, const package_model::Package* package,
                             const package_model::WatchDependency* watched);

/// 更新: タグ・main の札 (`v2.3.0 (41 先)` / `main +41 (うち merged 3)`)。 載せるものが無ければ ""。
std::string update_label(const package_model::WatchDependency& watched);

/// 由来の短い名前 (Pf / AI / 人 / 不明)。
std::string origin_text(const std::string& origin);

/// 審査中・作業中 1 件: `#128 Pf` / `run AI` と依存・head。
std::string review_label(const package_model::WatchUpdate& update);

/// 由来ごとに畳んだ審査中・作業中: 件数と番号 (先頭から数件)。
std::string folded_review_label(const std::string& origin,
                                const std::vector<const package_model::WatchUpdate*>& items);

/// 検査: ID と対象。
std::string check_label(const package_model::Finding& finding);

} // namespace ergo::package_viz::detail
