#pragma once
/// Wire records of the "log" plugin <-> ergo_log_live (port of log/protocol.ts).
/// Spec: spec/feature/tool/log.md. Holds the guards applied to untrusted
/// engine frames and the parser of ergo_log's console format.
#include "../common/json_util.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::log {

/// ergo_log level tags, most severe first.
bool is_level_tag(const std::string& s);

struct LogLine {
    std::string lvl;      ///< ERR / WRN / INF / DBG
    double      frame = 0;
    double      ts_us = 0;
    double      tid   = 0;
    std::string msg;
};

/// A stored line: `seq` numbers the lines of one app (1, 2, ...) and never
/// repeats, also across engine reconnects.
struct StoredLine : LogLine {
    uint64_t seq = 0;
};

JsonValue to_json_value(const StoredLine& l);

/// Longest msg kept, in UTF-8 bytes (= ergo::log::LiveSink::kMaxMessageBytes).
constexpr std::size_t kMaxMsgBytes = 8192;

/// Cut to kMaxMsgBytes on a UTF-8 character boundary and append U+2026.
std::string clip_message(const std::string& msg);

/// One engine line, or nullopt when it is not a well-formed record.
std::optional<LogLine> sanitize_line(const JsonValue& raw);

struct LinesBody {
    std::string          app;       ///< "" when the frame names none
    uint64_t             dropped = 0;
    std::vector<LogLine> lines;     ///< malformed lines skipped
};
/// A `lines` frame; nullopt when `lines` is not an array.
std::optional<LinesBody> sanitize_lines_msg(const JsonValue& msg);

/// ergo_log console output: `[F%08llu][TAG] msg`.
struct ConsoleLine {
    std::string lvl;
    uint64_t    frame = 0;
    std::string msg;
};
/// Trailing "\n" / "\r\n" / "\r" ignored; nullopt for any other format.
std::optional<ConsoleLine> parse_console_line(const std::string& text);

} // namespace ergo::studio::plugins::log
