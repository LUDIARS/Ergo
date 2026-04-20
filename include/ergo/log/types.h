#pragma once

#include <cstdint>
#include <string_view>

namespace ergo::log {

/// Lower value = more severe.
enum class Level : uint8_t {
    Error = 0,
    Warn  = 1,
    Info  = 2,
    Debug = 3,
};

/// Short tag used in log-line prefix: "ERR" / "WRN" / "INF" / "DBG".
constexpr std::string_view level_tag(Level l) {
    switch (l) {
        case Level::Error: return "ERR";
        case Level::Warn:  return "WRN";
        case Level::Info:  return "INF";
        case Level::Debug: return "DBG";
    }
    return "?";
}

} // namespace ergo::log
