#pragma once
/// net::HttpResponse on the wire: one response per connection (Connection: close),
/// Content-Length from the body, and only header names / values Portus accepts
/// (a value with a line break would split the response, so it is dropped).
#include "ergo/studio/net/http_message.h"

#include <string>

namespace ergo::studio::web {

std::string wire_response(const net::HttpResponse& res, bool head_only = false);

/// A refusal before any handler ran: {"ok":false,"err":<why>}.
std::string wire_refusal(int status, const std::string& why);

} // namespace ergo::studio::web
