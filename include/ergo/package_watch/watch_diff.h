#pragma once

// 前回状態と今回の観測から、 新しい出来事と次の状態を作る (spec/feature/package-manager.md §3)。
// 純粋関数で、 時刻は引数で受ける。 同じ入力なら同じ結果 (入力の並びにもよらない)。
//
//   * 同じ (host, 依存, kind, 主体) は 1 回だけ出す。 既知の未解消は次の状態に残る (pending)
//   * track は同じ依存で kTrackFoldSeconds に 1 回へ畳む。 出すときは最後の通知からの距離の差を添える
//   * 今回の判定に無いもの (pin を上げて解消したもの) は状態から消す
//   * 観測できなかった依存は、 前回の既知集合と指紋をそのまま残す
//   * 宣言から消えた依存は状態から消す

#include "ergo/package_watch/detect.h"
#include "ergo/package_watch/observation.h"
#include "ergo/package_watch/watch_dependency.h"
#include "ergo/package_watch/watch_state.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ergo::package_watch {

constexpr std::int64_t kTrackFoldSeconds = 6 * 60 * 60;

/// 依存 1 つの今回の結果。
struct DependencyResult {
    WatchDependency        dependency;
    Observation            observation;
    std::vector<Candidate> updates;  ///< detect_updates の結果
};

struct WatchEvent {
    std::string        id;  ///< "<host>/<依存>/<kind>/<主体>"
    std::string        host;
    WatchDependency    dependency;
    Candidate          update;
    std::string        as_of;
    std::int64_t       at = 0;
    std::optional<int> since_notified;  ///< track: 前回の通知からの距離の差
};

struct DiffResult {
    std::vector<WatchEvent> events;  ///< (依存, kind, 主体) の順
    WatchState              next;
};

/// 依存 1 つ分の結果 (detect_updates を当てる)。
DependencyResult make_result(const WatchDependency& dep, const Observation& obs);

std::string event_id(const std::string& host, const std::string& dependency, UpdateKind kind,
                     const std::string& subject);

DiffResult diff(const WatchState& previous, const std::string& host,
                const std::vector<DependencyResult>& results, std::int64_t now);

} // namespace ergo::package_watch
