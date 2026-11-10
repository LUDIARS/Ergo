#pragma once
/// "pictor_perf" plugin: keeps the last Pictor PerfQueryAPI::export_json()
/// snapshot, published over /pictor_perf/ws or POST /pictor_perf/api/publish
/// (headless hosts). A snapshot must be a JSON object with "memory".
#include "ergo/studio/host/plugin.h"

#include <memory>
#include <string>

namespace ergo::studio::plugins::pictor_perf {

/// True for a JSON object that has a "memory" field.
bool is_perf_snapshot(const std::string& doc);

std::unique_ptr<host::IPlugin> make_pictor_perf_plugin(const host::LogFn& log);

} // namespace ergo::studio::plugins::pictor_perf
