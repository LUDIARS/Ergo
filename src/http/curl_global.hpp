#pragma once

/// Internal: process-wide libcurl initialisation. Not part of the public API.
///
/// `curl_global_init` is not thread-safe and must run once before any easy
/// handle is used; this serialises that into a single guarded call.

namespace ergo::http::detail {

/// Ensure `curl_global_init` has run exactly once for this process. The matching
/// `curl_global_cleanup` runs at static-destruction time. Safe to call from any
/// thread and any number of times.
void ensure_curl_global_init();

} // namespace ergo::http::detail
