#pragma once

/// ergo_package_model — P4 ホスト状態図の入力の読み手 (host_state.h)。
///
/// 報告と状態は schema 1 だけを受け付ける。 壊れた・別 schema のファイルは「無い」と同じ扱いにし、
/// 理由を notes に残す (図は出す。 spec/feature/package-manager.md §5.1)。

#include "ergo/package_model/host_state.h"

#include <string>

namespace ergo::package_model {

/// `watch-report.json` の本文を読む。 壊れている・schema が 1 でなければ false と理由。
bool read_watch_report_text(const std::string& json, WatchReport& out, std::string* error = nullptr);

/// `ergo-packages.status.json` の本文を読む。 壊れている・schema が 1 でなければ false と理由。
bool read_host_status_text(const std::string& json, HostStatus& out, std::string* error = nullptr);

/// 読む場所。 空のものは読まない (無いのと同じ)。
struct HostStatePaths {
    std::string resolved;   ///< <build>/ergo-packages.resolved.json
    std::string report;     ///< <host>/.ergo/watch-report.json
    std::string status;     ///< <build>/ergo-packages.status.json
};

/// 3 つを読む。 失敗しない (欠けたもの・読めなかったものは has_* が false で notes に理由)。
HostState load_host_state(const HostStatePaths& paths);

} // namespace ergo::package_model
