#pragma once

// 前回状態 (spec/feature/package-manager.md §3「前回状態と重複の抑止」、 形式は spec/data/ergo-watch-report.md)。
// ホストごとに 1 ファイル。 リポの外 (--state、 既定は利用者ディレクトリ) にだけ置く。

#include "ergo/common/json_min.h"
#include "ergo/package_watch/detect.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::package_watch {

namespace jsonm = ergo::common::jsonm;

struct StateEntry {
    Candidate          update;
    std::int64_t       first_seen    = 0;  ///< UTC 秒
    std::int64_t       last_notified = 0;  ///< UTC 秒
    std::optional<int> notified_behind;    ///< track: 最後に通知したときの距離
};

struct WatchState {
    std::string                        host;
    std::map<std::string, std::string> fingerprints;  ///< 依存名 → 指紋
    std::vector<StateEntry>            entries;       ///< (依存, kind, 主体) の順
};

/// (依存, kind, 主体) の順に並べる。
void sort_entries(std::vector<StateEntry>& entries);

jsonm::JsonValue candidate_to_json(const Candidate& c);
bool             read_candidate(const jsonm::JsonValue& doc, Candidate& out);

jsonm::JsonValue state_to_json(const WatchState& state);

/// 形が違えば false と理由。
bool read_state(const jsonm::JsonValue& doc, WatchState& out, std::string* error);

} // namespace ergo::package_watch
