#pragma once
/// Internal to ergo_studio_platform (non-Windows with libcurl): the libcurl
/// IHttpClient with an explicit response size limit, so tests can exercise
/// the limit without megabytes of traffic. make_system_http_client() uses
/// detail::kMaxResponseBytes.
#include "ergo/studio/platform/http_client.h"

#include <cstddef>
#include <memory>

namespace ergo::studio::platform::detail {

std::shared_ptr<IHttpClient> make_curl_http_client(std::size_t max_response_bytes);

} // namespace ergo::studio::platform::detail
