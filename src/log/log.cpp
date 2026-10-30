#include "ergo/log/log.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ergo::log {

Sink::~Sink() = default;

namespace {

/// The default sink: today's `[F%08llu][LVL] msg` line, Error / Warn to
/// stderr and Info / Debug to stdout, serialized by print_mtx.
class ConsoleSink final : public Sink {
public:
    void on_record(const Record& r) override {
        const auto tag = level_tag(r.level);
        std::FILE* out = (r.level == Level::Error || r.level == Level::Warn) ? stderr : stdout;
        std::lock_guard<std::mutex> g(print_mtx_);
        std::fprintf(out, "[F%08llu][%.*s] %.*s\n",
                     static_cast<unsigned long long>(r.frame),
                     static_cast<int>(tag.size()), tag.data(),
                     static_cast<int>(r.msg.size()), r.msg.data());
        std::fflush(out);
    }

private:
    std::mutex print_mtx_;
};

using SinkList = std::vector<Sink*>;

struct State {
    std::atomic<Level> threshold{Level::Info};
    FrameProvider      frame_provider = nullptr;
    TimeProvider       time_provider  = nullptr;
    const std::chrono::steady_clock::time_point epoch = std::chrono::steady_clock::now();

    ConsoleSink console;

    // The installed sinks. A log call copies the pointer under sinks_mtx and
    // walks the list without any lock; add / remove publish a new list
    // instead of mutating the shared one.
    std::mutex                      sinks_mtx;  // guards `sinks` and `slot`
    std::shared_ptr<const SinkList> sinks = std::make_shared<const SinkList>(SinkList{&console});
    // Dispatches in progress, counted in the slot that was current when
    // they took their list. remove_sink flips `slot` and waits for the old
    // one to drain, so new log calls cannot keep it waiting (two slots).
    unsigned                        slot = 0;
    std::atomic<int>                in_flight[2] = {{0}, {0}};
    std::mutex                      remove_mtx;  // one remove_sink drain at a time
};

State& state() {
    static State s;
    return s;
}

/// True while this thread is inside Sink::on_record(): log calls made
/// there are dropped instead of recursing into the sinks.
thread_local bool t_in_sink = false;

uint64_t current_frame() {
    auto& s = state();
    if (!s.frame_provider) return 0;
    // Protect against exceptions / user-supplied callbacks.
    try {
        return s.frame_provider();
    } catch (...) {
        return 0;
    }
}

int64_t current_time_us() {
    auto& s = state();
    if (s.time_provider) {
        try {
            return s.time_provider();
        } catch (...) {
            return 0;
        }
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - s.epoch).count();
}

uint64_t current_tid() {
    // Same id as ergo_profile's Event::tid, so a log line can be matched
    // with the timeline lane it was emitted from.
    thread_local const uint64_t tid =
        static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return tid;
}

/// Marks this thread as inside a sink for the dispatch's lifetime and
/// releases the dispatch's in-flight slot, even if a sink throws.
class DispatchScope {
public:
    explicit DispatchScope(std::atomic<int>& counter) : counter_(counter) { t_in_sink = true; }
    ~DispatchScope() {
        t_in_sink = false;
        counter_.fetch_sub(1, std::memory_order_release);
    }
    DispatchScope(const DispatchScope&)            = delete;
    DispatchScope& operator=(const DispatchScope&) = delete;

private:
    std::atomic<int>& counter_;
};

void dispatch(const Record& rec) {
    auto& s = state();
    std::shared_ptr<const SinkList> list;
    unsigned slot = 0;
    {
        std::lock_guard<std::mutex> g(s.sinks_mtx);
        list = s.sinks;
        slot = s.slot;
        s.in_flight[slot].fetch_add(1, std::memory_order_relaxed);
    }
    DispatchScope scope(s.in_flight[slot]);
    for (Sink* sink : *list) {
        // A throwing sink must not starve the ones after it.
        try {
            sink->on_record(rec);
        } catch (...) {
        }
    }
}

/// Publish a copy of the sink list without `sink`. Caller holds sinks_mtx.
bool erase_locked(State& s, Sink* sink) {
    const SinkList& cur = *s.sinks;
    if (std::find(cur.begin(), cur.end(), sink) == cur.end()) return false;
    SinkList next;
    next.reserve(cur.size() - 1);
    for (Sink* k : cur) {
        if (k != sink) next.push_back(k);
    }
    s.sinks = std::make_shared<const SinkList>(std::move(next));
    return true;
}

} // namespace

void set_level(Level threshold) {
    state().threshold.store(threshold, std::memory_order_relaxed);
}

Level level() {
    return state().threshold.load(std::memory_order_relaxed);
}

void set_frame_provider(FrameProvider fn) {
    state().frame_provider = std::move(fn);
}

void set_time_provider(TimeProvider fn) {
    state().time_provider = std::move(fn);
}

void add_sink(Sink* sink) {
    if (!sink) return;
    auto& s = state();
    std::lock_guard<std::mutex> g(s.sinks_mtx);
    const SinkList& cur = *s.sinks;
    if (std::find(cur.begin(), cur.end(), sink) != cur.end()) return;
    SinkList next = cur;
    next.push_back(sink);
    s.sinks = std::make_shared<const SinkList>(std::move(next));
}

void remove_sink(Sink* sink) {
    if (!sink) return;
    auto& s = state();
    if (t_in_sink) {
        // This thread is itself counted in a slot: waiting would deadlock.
        std::lock_guard<std::mutex> g(s.sinks_mtx);
        erase_locked(s, sink);
        return;
    }
    std::lock_guard<std::mutex> rg(s.remove_mtx);
    unsigned old_slot = 0;
    {
        std::lock_guard<std::mutex> g(s.sinks_mtx);
        if (!erase_locked(s, sink)) return;
        old_slot = s.slot;
        s.slot ^= 1u;
    }
    // Every dispatch that may still hold a list containing `sink` counted
    // itself in old_slot (the previous remove drained the other one), and
    // no new dispatch joins it.
    while (s.in_flight[old_slot].load(std::memory_order_acquire) != 0) {
        std::this_thread::yield();
    }
}

Sink& console_sink() {
    return state().console;
}

void vlog(Level lvl, const char* fmt, std::va_list args) {
    if (t_in_sink) return;  // logged from inside a sink: drop, never recurse
    auto& s = state();
    if (static_cast<int>(lvl) > static_cast<int>(s.threshold.load(std::memory_order_relaxed))) return;
    if (!fmt) return;

    // Render payload once into a stack buffer, grow if truncated.
    char stack_buf[512];
    const char* buf = stack_buf;
    std::size_t len = 0;
    std::string heap_buf;

    std::va_list copy;
    va_copy(copy, args);
    const int needed = std::vsnprintf(stack_buf, sizeof(stack_buf), fmt, copy);
    va_end(copy);

    if (needed < 0) {
        len = 0;  // encoding error: emit an empty payload
    } else if (static_cast<size_t>(needed) >= sizeof(stack_buf)) {
        heap_buf.resize(static_cast<size_t>(needed) + 1);
        std::vsnprintf(heap_buf.data(), heap_buf.size(), fmt, args);
        buf = heap_buf.data();
        len = static_cast<size_t>(needed);
    } else {
        len = static_cast<size_t>(needed);
    }

    const Record rec{lvl, current_frame(), current_time_us(), current_tid(),
                     std::string_view(buf, len)};
    dispatch(rec);
}

void log(Level lvl, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    vlog(lvl, fmt, args);
    va_end(args);
}

} // namespace ergo::log
