#pragma once
/// Incremental HTTP/1.1 request parser. Bytes are fed as they arrive; the
/// parser reports when a full request (head + Content-Length body) is ready.
/// Chunked request bodies are refused (no Studio client sends them).
#include "ergo/studio/net/http_message.h"

#include <cstddef>
#include <string>

namespace ergo::studio::net {

struct HttpLimits {
    std::size_t max_head = 64 * 1024;          // request line + headers
    std::size_t max_body = 32 * 1024 * 1024;   // same cap as WS payloads
    /// Header lines + query parameters of one request (Node's http default
    /// maxHeadersCount, the limit of the TS server this replaces). More is 431.
    std::size_t max_fields = 2000;
};

enum class ParseState { NeedMore, Complete, Error };

class HttpRequestParser {
public:
    explicit HttpRequestParser(HttpLimits limits = {}) : limits_(limits) {}

    /// Append received bytes and try to complete the request.
    ParseState feed(const char* data, std::size_t len);

    /// Valid after Complete.
    const HttpRequest& request() const { return req_; }
    /// HTTP status to answer with after Error (400 / 413 / 431 / 501).
    /// Content-Length must be decimal digits only; one over max_body is 413.
    int error_status() const { return error_status_; }
    /// Bytes received after the complete request (e.g. WS frames sent right
    /// after the upgrade request). Valid after Complete.
    std::string take_leftover();

private:
    ParseState fail(int status);
    /// 0 when the head is valid, else the HTTP status to answer with.
    int        parse_head(const std::string& head);

    HttpLimits  limits_;
    std::string buf_;
    bool        head_done_   = false;
    std::size_t body_len_    = 0;
    std::size_t body_start_  = 0;
    int         error_status_ = 400;
    HttpRequest req_;
};

} // namespace ergo::studio::net
