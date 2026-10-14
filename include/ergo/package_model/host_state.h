#pragma once

/// ergo_package_model — 1 ホストの今の状態 (P4 ホスト状態図の入力。 読むだけ)。
///
/// 入力は 3 つで、 どれが欠けても作れる (欠けたものは has_* が false、 図の札は "—"):
///   * `<build>/ergo-packages.resolved.json`   構成の結果 (PackageModel)
///   * `<host>/.ergo/watch-report.json`        ergo_pkg watch の報告 (spec/data/ergo-watch-report.md)
///   * `<build>/ergo-packages.status.json`     ergo_pkg status の状態 (spec/data/ergo-packages-status.md)
/// 報告と状態の書き手は ergo_package_watch / ergo_package_manager にあるが、 このモジュールは
/// それらを import しない (共有するのは形式だけ。 spec/feature/package-manager.md §5.1)。

#include "ergo/package_model/package_model.h"

#include <optional>
#include <string>
#include <vector>

namespace ergo::package_model {

/// 報告の更新 1 件のうち図が使うもの。
struct WatchUpdate {
    std::string                dependency;
    std::string                kind;      ///< tag / track / off-main / run / review / merged
    std::string                subject;
    std::string                to_tag;
    std::optional<long long>   behind;    ///< null は不明
    std::string                origin;    ///< run / review / merged: pf / ai / human / unknown
    std::optional<long long>   review;    ///< review / merged: local PR の番号
    std::string                head;      ///< review / merged: head の SHA7
};

/// 報告の依存 1 つ (`dependencies[]`)。
struct WatchDependency {
    std::string                name;
    std::string                code;
    std::string                owner;
    bool                       is_private = false;
    bool                       observed   = false;
    std::string                reason;
    std::string                pin_rev;
    std::string                pin_tag;
    std::string                track_branch;
    std::optional<long long>   track_behind;
    std::optional<bool>        on_track;
    std::vector<WatchUpdate>   updates;
};

/// `<host>/.ergo/watch-report.json` のうち図が使うもの。
struct WatchReport {
    std::string                  host;
    std::string                  generated_at;
    std::vector<WatchDependency> dependencies;   ///< 名前順
    std::string                  revisor;        ///< ok / unavailable / off ("" = 無い)
    std::string                  concordia;

    const WatchDependency* find(const std::string& name) const;
};

/// テスト結果の要約。 source が "unknown" なら数は意味を持たない (未実行)。
struct TestSummary {
    std::string source = "unknown";   ///< junit / last-test-log / unknown
    long long   total   = 0;
    long long   passed  = 0;
    long long   failed  = 0;
    long long   skipped = 0;
};

/// `<build>/ergo-packages.status.json`。
struct HostStatus {
    std::string host;
    std::string head;            ///< ホストの HEAD (40 桁。 取れなければ "")
    std::string configured_at;   ///< CMakeCache.txt の更新時刻 (UTC。 無ければ "")
    TestSummary tests;
};

struct HostState {
    bool                     has_resolved = false;
    PackageModel             resolved;
    bool                     has_report = false;
    WatchReport              report;
    bool                     has_status = false;
    HostStatus               status;
    std::vector<std::string> notes;   ///< 欠けた・読めなかった入力の説明 (図は出す)
};

} // namespace ergo::package_model
