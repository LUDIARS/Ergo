#pragma once
/// Where 保存 writes a recording: <ERGO_SONUS_RECORDING_DIR>/sonus-<UTC time>-<n>.snrc.
/// The path is never taken from a request (the routes cannot write elsewhere).
#include <ctime>
#include <string>

namespace ergo::studio::plugins::sonus {

/// "sonus-20261119-083015-3.snrc" for `t` (UTC) and the `n`-th save of this run.
std::string recording_file_name(std::time_t t, unsigned n);

} // namespace ergo::studio::plugins::sonus
