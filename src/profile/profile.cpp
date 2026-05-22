#include "ergo/profile/profile.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <ios>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#  include <windows.h>
#  include <psapi.h>
#else
#  include <unistd.h>
#endif

namespace ergo::profile {
namespace {

using Clock = std::chrono::steady_clock;

int64_t clock_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               Clock::now().time_since_epoch())
        .count();
}

std::atomic<bool>    g_enabled{true};
std::atomic<int64_t> g_epoch_ns{clock_ns()};

/// Per-thread event buffer. Intentionally leaked (one per recording thread)
/// so collected events stay valid for export across the process lifetime.
struct ThreadBuffer {
    uint64_t           tid;
    std::vector<Event> events;
};

std::mutex                                g_mutex;        // registry + names
std::vector<ThreadBuffer*>                g_buffers;
std::unordered_map<uint64_t, std::string> g_thread_names;

uint64_t current_tid() {
    return static_cast<uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

ThreadBuffer& tls_buffer() {
    thread_local ThreadBuffer* buf = [] {
        auto* b = new ThreadBuffer{current_tid(), {}};
        b->events.reserve(4096);
        std::lock_guard<std::mutex> lk(g_mutex);
        g_buffers.push_back(b);
        return b;
    }();
    return *buf;
}

uint64_t current_pid() {
#if defined(_WIN32)
    return static_cast<uint64_t>(::GetCurrentProcessId());
#else
    return static_cast<uint64_t>(::getpid());
#endif
}

/// Append `s` to `out` as a JSON string body (no surrounding quotes).
void append_escaped(std::string& out, const char* s) {
    if (!s) return;
    for (const char* p = s; *p != '\0'; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
}

} // namespace

void set_enabled(bool on) { g_enabled.store(on, std::memory_order_relaxed); }
bool is_enabled()         { return g_enabled.load(std::memory_order_relaxed); }

void begin_session() {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_epoch_ns.store(clock_ns(), std::memory_order_relaxed);
    for (auto* b : g_buffers) b->events.clear();
}

void clear() {
    std::lock_guard<std::mutex> lk(g_mutex);
    for (auto* b : g_buffers) b->events.clear();
}

std::size_t event_count() {
    std::lock_guard<std::mutex> lk(g_mutex);
    std::size_t n = 0;
    for (const auto* b : g_buffers) n += b->events.size();
    return n;
}

int64_t now_us() {
    return (clock_ns() - g_epoch_ns.load(std::memory_order_relaxed)) / 1000;
}

void record_instant(const char* name) {
    if (!is_enabled()) return;
    ThreadBuffer& b = tls_buffer();
    b.events.push_back(Event{name, Phase::instant, now_us(), 0, b.tid, 0.0});
}

void record_counter(const char* name, double value) {
    if (!is_enabled()) return;
    ThreadBuffer& b = tls_buffer();
    b.events.push_back(Event{name, Phase::counter, now_us(), 0, b.tid, value});
}

void record_complete(const char* name, int64_t begin_us, int64_t dur_us) {
    if (!is_enabled()) return;
    ThreadBuffer& b = tls_buffer();
    b.events.push_back(
        Event{name, Phase::complete, begin_us, dur_us, b.tid, 0.0});
}

void set_thread_name(const char* name) {
    if (!name) return;
    const uint64_t tid = tls_buffer().tid;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_thread_names[tid] = name;
}

uint64_t process_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<uint64_t>(pmc.WorkingSetSize);
    }
    return 0;
#elif defined(__linux__)
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (f == nullptr) return 0;
    long pages_total = 0;
    long pages_rss   = 0;
    const int n = std::fscanf(f, "%ld %ld", &pages_total, &pages_rss);
    std::fclose(f);
    if (n != 2) return 0;
    const long page = ::sysconf(_SC_PAGESIZE);
    return static_cast<uint64_t>(pages_rss) * static_cast<uint64_t>(page);
#else
    return 0;
#endif
}

ScopedMarker::ScopedMarker(const char* name)
    : name_(name), begin_us_(now_us()) {}

ScopedMarker::~ScopedMarker() {
    record_complete(name_, begin_us_, now_us() - begin_us_);
}

std::string export_chrome_trace() {
    std::lock_guard<std::mutex> lk(g_mutex);
    const uint64_t pid = current_pid();
    std::string out;
    out.reserve(64 * 1024);
    out += "{\"traceEvents\":[";
    bool first = true;
    const auto sep = [&out, &first] {
        if (!first) out += ',';
        first = false;
    };

    for (const auto* b : g_buffers) {
        for (const Event& e : b->events) {
            sep();
            out += "{\"name\":\"";
            append_escaped(out, e.name);
            out += "\",\"pid\":";
            out += std::to_string(pid);
            out += ",\"tid\":";
            out += std::to_string(e.tid);
            out += ",\"ts\":";
            out += std::to_string(e.ts_us);
            switch (e.phase) {
                case Phase::complete:
                    out += ",\"ph\":\"X\",\"cat\":\"ergo\",\"dur\":";
                    out += std::to_string(e.dur_us);
                    break;
                case Phase::instant:
                    out += ",\"ph\":\"i\",\"s\":\"t\"";
                    break;
                case Phase::counter: {
                    char vbuf[32];
                    std::snprintf(vbuf, sizeof(vbuf), "%.6g", e.value);
                    out += ",\"ph\":\"C\",\"args\":{\"";
                    append_escaped(out, e.name);
                    out += "\":";
                    out += vbuf;
                    out += "}";
                    break;
                }
            }
            out += "}";
        }
    }
    for (const auto& kv : g_thread_names) {
        sep();
        out += "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":";
        out += std::to_string(pid);
        out += ",\"tid\":";
        out += std::to_string(kv.first);
        out += ",\"args\":{\"name\":\"";
        append_escaped(out, kv.second.c_str());
        out += "\"}}";
    }
    out += "]}";
    return out;
}

bool dump(const std::string& path) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    const std::string json = export_chrome_trace();
    f.write(json.data(), static_cast<std::streamsize>(json.size()));
    return static_cast<bool>(f);
}

} // namespace ergo::profile
