#pragma once

// 報告と出来事の書き手 (形式は spec/data/ergo-watch-report.md、 schema 1)。
//
//   <host>/.ergo/watch-report.json  ホストごとの報告 (依存ごとの観測・今の更新・今回の出来事・未解消)
//   <out>/events.json               今回の新しい出来事 (全ホスト。 配送ジョブが読む)
//
// 載せないもの (§8): パス・URL・ポート・トークン・PR のタイトルと本文・ブランチ名・セッション id。 取得元の URL は報告に入れず、 非公開パッケージは
// private: true にする。 書く前に漏れ検査 (ergo_package_manager の find_leaks) を通し、 当たれば何も書かない。

#include "ergo/common/json_min.h"
#include "ergo/package_manager/leak_check.h"
#include "ergo/package_watch/provenance.h"
#include "ergo/package_watch/watch_diff.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ergo::package_watch {

namespace jsonm = ergo::common::jsonm;

/// ホスト 1 つの今回の結果。
struct HostWatch {
    std::string                   host;
    std::vector<DependencyResult> results;  ///< 依存名の順
    DiffResult                    diff;
    WatchSources                  sources;  ///< W4: Revisor / Concordia に届いたか
};

jsonm::JsonValue dependency_json(const DependencyResult& result);
jsonm::JsonValue event_json(const WatchEvent& event);

/// <host>/.ergo/watch-report.json の中身。
jsonm::JsonValue host_report_json(const HostWatch& watch, std::int64_t now);

/// <out>/events.json の中身。 sources は今回の Revisor / Concordia の状態 (全ホストで共通)。
jsonm::JsonValue events_json(const std::vector<HostWatch>& watches, std::int64_t now,
                             const WatchSources& sources = {});

/// `ergo_pkg outdated --json` の中身 (前回状態を持たない)。
jsonm::JsonValue outdated_json(const std::string& host, const std::vector<DependencyResult>& results,
                               std::int64_t now);

/// `ergo_pkg outdated` の表 (固定幅、 末尾改行)。
std::string render_outdated_text(const std::string& host, const std::vector<DependencyResult>& results);

struct PreparedFile {
    std::string host;     ///< 報告ならそのホスト、 events.json なら ""
    std::string content;  ///< 末尾改行つき
};

struct WatchOutputs {
    bool                                     ok = true;  ///< 漏れが無い
    std::vector<ergo::package_manager::Leak> leaks;      ///< where は "<host>:<JSON Pointer>" / "events:<...>"
    std::vector<PreparedFile>                reports;    ///< watches と同じ順
    PreparedFile                             events;
};

/// 全ての報告と events.json を作って漏れ検査を通す。 ok == false なら呼び手は何も書かない。
WatchOutputs prepare_watch_outputs(const std::vector<HostWatch>& watches, std::int64_t now,
                                   const WatchSources& sources = {});

} // namespace ergo::package_watch
