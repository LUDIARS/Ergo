#pragma once

/// LiveSink — streams ergo_log records to the unified ergo tool's `log`
/// plugin (tools/ergo/src/plugins/log/). Target `ergo_log_live`.
///
/// Connects out to `ws://host:5170/log/ws` over the shared transport
/// (`ergo_common_ws`), sends `{op:"hello", role:"engine", app}` on every
/// (re)connect and then batches the queued records as `lines` frames about
/// every 50 ms. Wire protocol: spec/feature/tool/log.md.
///
/// Typical host setup:
///
///     ergo::log::set_frame_provider(&ergo::frame::count);
///     ergo::log::set_time_provider(&ergo::profile::now_us);
///     static ergo::log::LiveSink live;
///     ergo::log::add_sink(&live);       // records queue from here on
///     live.start("127.0.0.1", 5170, "mygame");
///
/// on_record() only copies the record into a bounded queue (oldest records
/// are dropped when it is full, and the drop count is reported to the
/// tool), so logging never waits on the socket. Records queue while the
/// tool is down and are sent once it connects. At most two `lines` frames
/// sit in the socket's send queue at once, so a slow or stalled tool also
/// backs records up in this bounded queue rather than growing memory. The sink never logs through
/// ergo_log itself; failures surface through start()'s return value and
/// is_connected().
///
/// Release builds simply do not link this target (there is no dummy); the
/// core `ergo_log` library stays socket-free.

#include "ergo/log/log.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ergo::log {

/// A Record with its message copied: the element LiveSink queues.
struct OwnedRecord {
    Level       level = Level::Info;
    uint64_t    frame = 0;
    int64_t     ts_us = 0;
    uint64_t    tid   = 0;
    std::string msg;
};

/// `{"op":"hello","role":"engine","app":<app>}`
std::string encode_hello_frame(const std::string& app);

/// `{"op":"lines","app":<app>,"dropped":<n>,"lines":[{"lvl","frame","ts_us","tid","msg"}...]}`.
/// `lvl` is the level tag ("ERR" / "WRN" / "INF" / "DBG"); `dropped` counts
/// the records discarded since the previous frame. Built with jsonm, so
/// the numbers are JSON doubles (exact up to 2^53).
std::string encode_lines_frame(const std::string&              app,
                               const std::vector<OwnedRecord>& lines,
                               uint64_t                        dropped);

class LiveSink : public Sink {
public:
    static constexpr std::size_t kQueueCapacity   = 4096;  ///< default queue bound (records)
    static constexpr std::size_t kMaxBatchLines   = 512;   ///< records per `lines` frame
    static constexpr std::size_t kMaxMessageBytes = 8192;  ///< longer messages are cut ("…")
    static constexpr std::chrono::milliseconds kFlushInterval{50};

    explicit LiveSink(std::size_t capacity = kQueueCapacity);
    /// Uninstalls itself from ergo_log (remove_sink(this)) and stops.
    ~LiveSink() override;

    LiveSink(const LiveSink&)            = delete;
    LiveSink& operator=(const LiveSink&) = delete;

    /// Start connecting in the background; the link reconnects on its own
    /// whenever the tool restarts. `app` names this engine in the tool (two
    /// engines with the same name are shown as `app`, `app#2`, ...).
    /// Returns false if already started (call stop() first) or if host,
    /// port or app is empty. Does not install the sink: call add_sink().
    bool start(std::string host = "127.0.0.1", uint16_t port = 5170, std::string app = "app");

    /// Stop the worker and close the socket, without waiting on a tool that
    /// stopped reading. Records still in the queue stay queued for the next
    /// start(); up to two `lines` frames already handed to the socket may be
    /// lost. Safe to call repeatedly.
    void stop();

    /// True while the WebSocket session is open.
    bool is_connected() const;

    /// Copy `r` into the queue (any thread). Never blocks on the socket.
    void on_record(const Record& r) override;

    /// Records waiting in the queue.
    std::size_t pending() const;
    /// Records dropped because the queue was full, since construction.
    uint64_t    dropped() const;

    /// Take up to `max_lines` queued records (oldest first) and the drop
    /// count not reported yet, encoded as one `lines` frame. Returns an
    /// empty string when there is nothing to report. The worker calls this
    /// while connected; tests, or a host that owns its own transport, can
    /// call it instead of start().
    std::string take_frame(std::size_t max_lines = kMaxBatchLines);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ergo::log
