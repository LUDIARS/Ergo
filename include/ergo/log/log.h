#pragma once

/// ergo::log — 4-level developer logger with frame-number prefix.
///
/// Each line is emitted as:
///   [F<frame>][<LVL>] <message>
///
/// Levels (most to least severe): Error / Warn / Info / Debug.
/// Error and Warn go to stderr; Info and Debug go to stdout. A global
/// level filter drops anything below its threshold (default: Info).
///
/// The logger is thread-safe: each log call acquires an internal mutex
/// around the write so lines are never interleaved.
///
/// Frame number comes from an injectable callback. Default returns 0 so
/// the logger works standalone; wire it to `ergo_frame` via:
///
///   #include "ergo/frame/frame.h"
///   ergo::log::set_frame_provider(&ergo::frame::count);

#include <cstdarg>
#include <cstdint>
#include <functional>

#include "ergo/log/types.h"

namespace ergo::log {

using FrameProvider = std::function<uint64_t()>;

/// Drop messages with `level > threshold`. Default: Level::Info.
void  set_level(Level threshold);
Level level();

/// Inject a callable returning the current frame number. Called each
/// time a line is logged. Reset to the default (returns 0) by passing
/// an empty `FrameProvider{}`.
void set_frame_provider(FrameProvider fn);

/// Emit a formatted line. printf-compatible format string.
void log (Level lvl, const char* fmt, ...);
void vlog(Level lvl, const char* fmt, std::va_list args);

} // namespace ergo::log

// ---- Convenience macros -----------------------------------------------

#define ERGO_LOG_ERROR(fmt, ...) ::ergo::log::log(::ergo::log::Level::Error, fmt, ##__VA_ARGS__)
#define ERGO_LOG_WARN(fmt,  ...) ::ergo::log::log(::ergo::log::Level::Warn,  fmt, ##__VA_ARGS__)
#define ERGO_LOG_INFO(fmt,  ...) ::ergo::log::log(::ergo::log::Level::Info,  fmt, ##__VA_ARGS__)
#define ERGO_LOG_DEBUG(fmt, ...) ::ergo::log::log(::ergo::log::Level::Debug, fmt, ##__VA_ARGS__)
