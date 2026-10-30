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
///
/// Output goes through sinks. The console sink (the format above) is
/// installed by default; add_sink() adds more destinations (e.g. the
/// `ergo_log_live` LiveSink that streams to the tools/ergo `log` plugin),
/// and remove_sink(&console_sink()) silences the console in a GUI app.
/// Each record also carries a timestamp from an injectable time source;
/// wire it to `ergo_profile` so log lines line up with the timeline:
///
///   #include "ergo/profile/profile.h"
///   ergo::log::set_time_provider(&ergo::profile::now_us);

#include <cstdarg>
#include <cstdint>
#include <functional>
#include <string_view>

#include "ergo/log/types.h"

namespace ergo::log {

using FrameProvider = std::function<uint64_t()>;
using TimeProvider  = std::function<int64_t()>;

/// Drop messages with `level > threshold`. Default: Level::Info.
void  set_level(Level threshold);
Level level();

/// Inject a callable returning the current frame number. Called each
/// time a line is logged. Reset to the default (returns 0) by passing
/// an empty `FrameProvider{}`.
void set_frame_provider(FrameProvider fn);

/// Inject a callable returning the current time in microseconds (e.g.
/// `&ergo::profile::now_us`). Called each time a line is logged. Reset to
/// the default (steady_clock microseconds since the logger's first use) by
/// passing an empty `TimeProvider{}`.
///
/// Like set_frame_provider, call it during startup, before other threads
/// log: providers are not swapped atomically.
void set_time_provider(TimeProvider fn);

/// Emit a formatted line. printf-compatible format string.
void log (Level lvl, const char* fmt, ...);
void vlog(Level lvl, const char* fmt, std::va_list args);

// ---- Sinks ------------------------------------------------------------

/// One emitted line, as handed to every sink. Lines dropped by the level
/// filter never reach a sink. `msg` is the formatted payload without the
/// `[F..][LVL]` prefix; it points into a buffer owned by the log call and
/// is valid only during Sink::on_record() (copy it to keep it).
struct Record {
    Level            level;
    uint64_t         frame;  ///< frame provider value at emit time
    int64_t          ts_us;  ///< time provider value at emit time (µs)
    uint64_t         tid;    ///< emitting thread (hashed std::thread::id, as ergo_profile)
    std::string_view msg;
};

/// Destination for log records.
///
/// on_record() runs on the logging thread with no ergo_log lock held, so
/// implementations must be thread-safe. Log calls made from inside a sink
/// (on the same thread) are dropped, so a sink can never recurse into
/// itself; it may still call add_sink() / remove_sink().
class Sink {
public:
    virtual ~Sink();
    virtual void on_record(const Record& r) = 0;
};

/// Install `sink` (non-owning). Sinks are called in installation order.
/// Installing a sink twice, or nullptr, is a no-op. `sink` must stay alive
/// until it has been removed.
void add_sink(Sink* sink);

/// Uninstall `sink`. When this returns, no thread is still inside
/// `sink->on_record()`, so the sink may be destroyed right away — except
/// when called from inside a sink, where it only uninstalls and does not
/// wait. A sink that is not installed is a no-op.
void remove_sink(Sink* sink);

/// The built-in console sink (stdout / stderr, format above). Installed
/// by default; pass it to remove_sink() / add_sink() to toggle the console.
Sink& console_sink();

} // namespace ergo::log

// ---- Convenience macros -----------------------------------------------

#define ERGO_LOG_ERROR(fmt, ...) ::ergo::log::log(::ergo::log::Level::Error, fmt, ##__VA_ARGS__)
#define ERGO_LOG_WARN(fmt,  ...) ::ergo::log::log(::ergo::log::Level::Warn,  fmt, ##__VA_ARGS__)
#define ERGO_LOG_INFO(fmt,  ...) ::ergo::log::log(::ergo::log::Level::Info,  fmt, ##__VA_ARGS__)
#define ERGO_LOG_DEBUG(fmt, ...) ::ergo::log::log(::ergo::log::Level::Debug, fmt, ##__VA_ARGS__)
