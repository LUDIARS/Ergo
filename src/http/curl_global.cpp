#include "curl_global.hpp"

#include <curl/curl.h>

namespace ergo::http::detail {
namespace {

/// RAII wrapper: init on construction, cleanup on destruction. A single
/// function-local static instance gives us thread-safe once-only init (C++11
/// "magic statics") plus deterministic teardown at process exit.
struct CurlGlobal {
    CURLcode code;
    CurlGlobal() : code(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
    ~CurlGlobal() {
        if (code == CURLE_OK) {
            curl_global_cleanup();
        }
    }
    CurlGlobal(const CurlGlobal&)            = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;
};

} // namespace

void ensure_curl_global_init() {
    static CurlGlobal g;
    (void)g;
}

} // namespace ergo::http::detail
