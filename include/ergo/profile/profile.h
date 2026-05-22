#pragma once

/// ergo_profile - performance timeline via AOP-style marker injection.
///
/// Markers are the cross-cutting "profiling" concern, injected at scope
/// boundaries through RAII guards + macros. Recording is thread-local and
/// lock-free; the collector merges per-thread buffers on export. Output is
/// Chrome Trace Event JSON (viewable in chrome://tracing / Perfetto, or the
/// tools/ergo `profile` plugin).
///
/// Marker names must outlive the session (string literals / __func__) - the
/// collector stores the pointer, not a copy, to keep the hot path alloc-free.
///
/// Full spec: spec/module/profile.md

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace ergo::profile {

/// Marker kind. Maps to the Chrome Trace Event `ph` field.
enum class Phase : uint8_t {
    complete,  ///< 'X' - a scope with begin + duration (speed profiling)
    instant,   ///< 'i' - a single point in time
    counter,   ///< 'C' - a numeric sample (memory / counters)
};

/// One recorded marker. POD, collected flat in thread-local buffers.
struct Event {
    const char* name;     ///< marker name - not owned (must be a literal)
    Phase       phase;
    int64_t     ts_us;    ///< timestamp since session epoch, microseconds
    int64_t     dur_us;   ///< duration (complete only), microseconds
    uint64_t    tid;      ///< recording thread id (hashed std::thread::id)
    double      value;    ///< sample value (counter only)
};

// --- runtime control -------------------------------------------------------

/// Enable/disable recording at runtime (compile-time gate is the macros).
void   set_enabled(bool on);
bool   is_enabled();
/// Reset the session epoch and clear all recorded events.
void   begin_session();
/// Drop all recorded events (keeps the session epoch).
void   clear();
/// Total events recorded across all threads.
std::size_t event_count();

// --- recording (the macros call these; usable directly too) ----------------

void   record_instant(const char* name);
void   record_counter(const char* name, double value);
void   record_complete(const char* name, int64_t begin_us, int64_t dur_us);
/// Give the calling thread a display name for the timeline lane.
void   set_thread_name(const char* name);

/// Microseconds elapsed since the session epoch.
int64_t now_us();
/// Current process resident set size in bytes (0 when unavailable).
uint64_t process_rss_bytes();

// --- export ----------------------------------------------------------------

/// Serialize all recorded events to Chrome Trace Event JSON.
std::string export_chrome_trace();
/// Write export_chrome_trace() to `path`. Returns false on write failure.
bool dump(const std::string& path);

// --- RAII scope marker -----------------------------------------------------

/// Records a 'complete' event spanning its own lifetime.
/// Prefer the ERGO_PROFILE_SCOPE macro (zero-cost when profiling is off).
class ScopedMarker {
public:
    explicit ScopedMarker(const char* name);
    ~ScopedMarker();
    ScopedMarker(const ScopedMarker&)            = delete;
    ScopedMarker& operator=(const ScopedMarker&) = delete;

private:
    const char* name_;
    int64_t     begin_us_;
};

/// AOP advice - run `fn` wrapped in a scope marker; forwards fn's result.
template <typename Fn>
auto aspect(Fn&& fn, const char* name) -> decltype(fn()) {
    ScopedMarker guard{name};
    return std::forward<Fn>(fn)();
}

} // namespace ergo::profile

// --- markers (zero-cost when ERGO_PROFILE_ENABLED is unset) -----------------
//
// The ergo_profile target defines ERGO_PROFILE_ENABLED=1 (PUBLIC). When the
// module is not built (ERGO_BUILD_PROFILE=OFF) consumers never see the define,
// so every marker macro collapses to `((void)0)`.

#if defined(ERGO_PROFILE_ENABLED) && ERGO_PROFILE_ENABLED

#define ERGO_PROFILE_DETAIL_CONCAT_(a, b) a##b
#define ERGO_PROFILE_DETAIL_CONCAT(a, b)  ERGO_PROFILE_DETAIL_CONCAT_(a, b)

/// Profile the enclosing scope's duration (speed).
#define ERGO_PROFILE_SCOPE(name)                                            \
    ::ergo::profile::ScopedMarker ERGO_PROFILE_DETAIL_CONCAT(_ergo_prof_,    \
                                                            __LINE__){name}
/// Profile the enclosing function (uses __func__).
#define ERGO_PROFILE_FUNC()        ERGO_PROFILE_SCOPE(__func__)
/// Record an instant marker at the current time.
#define ERGO_PROFILE_MARK(name)    ::ergo::profile::record_instant(name)
/// Record a counter sample.
#define ERGO_PROFILE_COUNTER(name, v)                                       \
    ::ergo::profile::record_counter((name), static_cast<double>(v))
/// Sample the process RSS as a counter (memory).
#define ERGO_PROFILE_MEM(name)                                              \
    ::ergo::profile::record_counter(                                        \
        (name), static_cast<double>(::ergo::profile::process_rss_bytes()))
/// Name the calling thread for the timeline lane.
#define ERGO_PROFILE_THREAD(name)  ::ergo::profile::set_thread_name(name)

#else // profiling disabled - every marker is a no-op

#define ERGO_PROFILE_SCOPE(name)      ((void)0)
#define ERGO_PROFILE_FUNC()           ((void)0)
#define ERGO_PROFILE_MARK(name)       ((void)0)
#define ERGO_PROFILE_COUNTER(name, v) ((void)0)
#define ERGO_PROFILE_MEM(name)        ((void)0)
#define ERGO_PROFILE_THREAD(name)     ((void)0)

#endif
