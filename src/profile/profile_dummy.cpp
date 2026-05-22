// ergo_profile dummy plug — no-op definitions for link-only use.
//
// A target that links ergo_profile_dummy instead of ergo_profile resolves
// every ergo::profile symbol to a no-op. Note: without ERGO_PROFILE_ENABLED
// the marker macros already collapse to nothing, so this plug only matters
// for code that calls the ergo::profile API directly.

#include "ergo/profile/profile.h"

namespace ergo::profile {

void set_enabled(bool) {}
bool is_enabled() { return false; }
void begin_session() {}
void clear() {}
std::size_t event_count() { return 0; }

void record_instant(const char*) {}
void record_counter(const char*, double) {}
void record_complete(const char*, int64_t, int64_t) {}
void set_thread_name(const char*) {}

int64_t  now_us() { return 0; }
uint64_t process_rss_bytes() { return 0; }

std::string export_chrome_trace() { return "{\"traceEvents\":[]}"; }
bool dump(const std::string&) { return false; }

ScopedMarker::ScopedMarker(const char* name) : name_(name), begin_us_(0) {}
ScopedMarker::~ScopedMarker() {}

} // namespace ergo::profile
