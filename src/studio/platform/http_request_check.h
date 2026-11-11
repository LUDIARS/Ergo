#pragma once
/// Internal to ergo_studio_platform (non-Windows): what a request must look
/// like before the libcurl backend hands it to the network. Pure functions,
/// no libcurl, so they are unit-tested on their own.
///
/// libcurl speaks many protocols and puts header lines on the wire as given,
/// so the backend accepts only http:// and https:// URLs and refuses header
/// names / values that could split the request (CR, LF, NUL).
#include "ergo/studio/platform/http_client.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::platform::detail {

/// Upper bound of a response body. The Studio's callers exchange JSON with
/// APIs and local tools; a larger body is reported as an error instead of
/// filling memory.
constexpr std::size_t kMaxResponseBytes = 64u * 1024u * 1024u;

struct CheckedRequest {
    bool                     https = false;   ///< the scheme is https (redirects may then only go to https)
    bool                     is_get = false;
    bool                     is_post = false;
    std::vector<std::string> header_lines;    ///< "Name: value" ("Name;" for an empty value), then "Content-Type:" /
                                              ///< "Expect:" (= do not add one) unless the caller set them
};

struct RequestCheck {
    std::optional<CheckedRequest> request;   ///< set when the request may be sent
    std::string                   error;     ///< otherwise why not (never contains the URL)
};

/// Validate `req`: an absolute http:// or https:// URL with a host and no
/// whitespace or control characters; a method of upper-case letters; header
/// names of RFC 9110 token characters; header values without CR, LF or NUL;
/// timeout_ms > 0.
RequestCheck check_http_request(const HttpClientRequest& req);

/// Append `len` bytes to `body` unless that would pass `limit`. False (and
/// `body` unchanged) when it would.
bool append_capped(std::string& body, const char* data, std::size_t len, std::size_t limit);

} // namespace ergo::studio::platform::detail
