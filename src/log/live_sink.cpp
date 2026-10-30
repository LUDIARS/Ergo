#include "ergo/log/live_sink.h"

#include "ergo/common/json_min.h"
#include "ergo/common/ws_client.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>

namespace ergo::log {

namespace {

namespace jsonm = ::ergo::common::jsonm;
namespace ws    = ::ergo::common::ws;

constexpr const char* kPath = "/log/ws";

/// Copy `msg`, cut to kMaxMessageBytes on a UTF-8 boundary with a trailing
/// "…" so one huge line cannot bloat the queue or a frame.
std::string clip_message(std::string_view msg) {
    if (msg.size() <= LiveSink::kMaxMessageBytes) return std::string(msg);
    std::size_t cut = LiveSink::kMaxMessageBytes;
    while (cut > 0 && (static_cast<unsigned char>(msg[cut]) & 0xC0) == 0x80) --cut;
    std::string out(msg.substr(0, cut));
    out += "\xE2\x80\xA6";  // U+2026
    return out;
}

} // namespace

std::string encode_hello_frame(const std::string& app) {
    auto m = jsonm::JsonValue::make_object();
    m.set("op",   jsonm::JsonValue::make_string("hello"));
    m.set("role", jsonm::JsonValue::make_string("engine"));
    m.set("app",  jsonm::JsonValue::make_string(app));
    return jsonm::serialize(m);
}

std::string encode_lines_frame(const std::string&              app,
                               const std::vector<OwnedRecord>& lines,
                               uint64_t                        dropped) {
    auto arr = jsonm::JsonValue::make_array();
    for (const OwnedRecord& r : lines) {
        auto line = jsonm::JsonValue::make_object();
        line.set("lvl",   jsonm::JsonValue::make_string(std::string(level_tag(r.level))));
        line.set("frame", jsonm::JsonValue::make_number(static_cast<double>(r.frame)));
        line.set("ts_us", jsonm::JsonValue::make_number(static_cast<double>(r.ts_us)));
        line.set("tid",   jsonm::JsonValue::make_number(static_cast<double>(r.tid)));
        line.set("msg",   jsonm::JsonValue::make_string(r.msg));
        arr.push(std::move(line));
    }
    auto m = jsonm::JsonValue::make_object();
    m.set("op",      jsonm::JsonValue::make_string("lines"));
    m.set("app",     jsonm::JsonValue::make_string(app));
    m.set("dropped", jsonm::JsonValue::make_number(static_cast<double>(dropped)));
    m.set("lines",   std::move(arr));
    return jsonm::serialize(m);
}

// ===========================================================================
// Impl — the record queue plus the batching worker. The socket worker lives
// inside ws::Client; this worker only moves queued records into frames.
// ===========================================================================
struct LiveSink::Impl {
    explicit Impl(std::size_t cap) : capacity(std::max<std::size_t>(cap, 1)) {}

    const std::size_t capacity;

    // Guards the queue, the drop counters and `app`. Held only for copies,
    // never across a send, so on_record() stays cheap.
    mutable std::mutex      q_mtx;
    std::deque<OwnedRecord> queue;
    uint64_t                unreported    = 0;  // drops not sent in a frame yet
    uint64_t                dropped_total = 0;
    std::string             app = "app";

    std::mutex              wake_mtx;
    std::condition_variable wake_cv;
    bool                    stopping = false;  // guarded by wake_mtx
    std::thread             worker;
    bool                    started = false;   // start() / stop() caller only

    ws::Client              client;            // last: destroyed (and stopped) first

    void push(OwnedRecord rec) {
        std::lock_guard<std::mutex> lk(q_mtx);
        if (queue.size() >= capacity) {
            queue.pop_front();
            ++unreported;
            ++dropped_total;
        }
        queue.push_back(std::move(rec));
    }

    std::string take(std::size_t max_lines) {
        std::vector<OwnedRecord> batch;
        uint64_t                 dropped = 0;
        std::string              name;
        {
            std::lock_guard<std::mutex> lk(q_mtx);
            if (queue.empty() && unreported == 0) return {};
            const std::size_t n = std::min(max_lines, queue.size());
            batch.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                batch.push_back(std::move(queue.front()));
                queue.pop_front();
            }
            dropped    = unreported;
            unreported = 0;
            name       = app;
        }
        return encode_lines_frame(name, batch, dropped);
    }

    std::string app_name() const {
        std::lock_guard<std::mutex> lk(q_mtx);
        return app;
    }

    void wake() {
        { std::lock_guard<std::mutex> lk(wake_mtx); }
        wake_cv.notify_one();
    }

    void flush() {
        // A frame handed over while the session drops is lost with it.
        while (client.is_connected()) {
            std::string frame = take(kMaxBatchLines);
            if (frame.empty()) break;
            client.send_text(std::move(frame));
        }
    }

    void run() {
        std::unique_lock<std::mutex> lk(wake_mtx);
        while (!stopping) {
            wake_cv.wait_for(lk, kFlushInterval);
            if (stopping) break;
            lk.unlock();
            flush();
            lk.lock();
        }
    }
};

LiveSink::LiveSink(std::size_t capacity) : impl_(std::make_unique<Impl>(capacity)) {
    // Build the logger's state first, so a static LiveSink is destroyed (and
    // calls remove_sink) before that state is.
    (void)console_sink();
    Impl* impl = impl_.get();
    // Socket worker thread, after every handshake (reconnects included).
    impl_->client.set_on_open([impl] {
        impl->client.send_text(encode_hello_frame(impl->app_name()));
        impl->wake();  // send what queued up while disconnected right away
    });
}

LiveSink::~LiveSink() {
    remove_sink(this);  // no log call is inside on_record() after this
    stop();
}

bool LiveSink::start(std::string host, uint16_t port, std::string app) {
    if (impl_->started || host.empty() || port == 0 || app.empty()) return false;
    {
        std::lock_guard<std::mutex> lk(impl_->q_mtx);
        impl_->app = std::move(app);
    }
    {
        std::lock_guard<std::mutex> lk(impl_->wake_mtx);
        impl_->stopping = false;
    }
    impl_->started = true;
    Impl* impl = impl_.get();
    impl_->worker = std::thread([impl] { impl->run(); });
    impl_->client.start(std::move(host), port, kPath);
    return true;
}

void LiveSink::stop() {
    if (!impl_->started) return;
    {
        std::lock_guard<std::mutex> lk(impl_->wake_mtx);
        impl_->stopping = true;
    }
    impl_->wake_cv.notify_one();
    if (impl_->worker.joinable()) impl_->worker.join();
    impl_->client.stop();
    impl_->started = false;
}

bool LiveSink::is_connected() const { return impl_->client.is_connected(); }

void LiveSink::on_record(const Record& r) {
    impl_->push(OwnedRecord{r.level, r.frame, r.ts_us, r.tid, clip_message(r.msg)});
}

std::size_t LiveSink::pending() const {
    std::lock_guard<std::mutex> lk(impl_->q_mtx);
    return impl_->queue.size();
}

uint64_t LiveSink::dropped() const {
    std::lock_guard<std::mutex> lk(impl_->q_mtx);
    return impl_->dropped_total;
}

std::string LiveSink::take_frame(std::size_t max_lines) { return impl_->take(max_lines); }

} // namespace ergo::log
