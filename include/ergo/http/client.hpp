#pragma once

/// ergo::http — minimal synchronous HTTP client.
///
/// A thin, dependency-hiding surface for "fetch this URL" use cases. The public
/// API is pure `std::` types: consumers link `ergo_http` and never see libcurl.
/// The concrete client is libcurl-backed (`make_curl_client()`); callers depend
/// on the `IHttpClient` interface so the transport can be swapped or faked.
///
/// Not in scope: streaming/chunked bodies, async, connection pooling, cookies,
/// multipart. Modules that need those build on top of (or beside) this.

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ergo::http {

/// HTTP verbs we support. Deliberately closed — add cases here, not via flags.
enum class Method { Get, Post };

/// Ordered list of header name/value pairs (HTTP allows duplicates).
using Headers = std::vector<std::pair<std::string, std::string>>;

/// A request to send. Only `url` is mandatory; the rest have sane defaults.
struct Request {
    std::string url;
    Method      method          = Method::Get;
    std::string body;                          ///< request body (POST)
    std::string content_type;                  ///< sets Content-Type when non-empty
    Headers     headers;                       ///< extra request headers
    long        timeout_ms       = 0;          ///< 0 = no explicit timeout
    bool        follow_redirects = true;
};

/// The outcome of a send.
struct Response {
    bool        ok     = false;  ///< true if the transfer completed (any HTTP status)
    long        status = 0;      ///< HTTP status code (0 if the transfer never completed)
    std::string body;            ///< response body
    Headers     headers;         ///< response headers
    std::string error;           ///< transport error text when ok == false
};

/// Transport interface. `send()` is the single primitive; `get()`/`post()` are
/// convenience wrappers so the interface stays small (ISP) while callers depend
/// on the abstraction, not the libcurl concrete (DIP).
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /// Perform `req` synchronously. Never throws; failures land in
    /// `Response::ok == false` with `Response::error` set.
    virtual Response send(const Request& req) = 0;

    Response get(const std::string& url, const Headers& headers = {}) {
        Request req;
        req.url     = url;
        req.method  = Method::Get;
        req.headers = headers;
        return send(req);
    }

    Response post(const std::string& url,
                  std::string        body,
                  std::string        content_type = "application/octet-stream",
                  const Headers&     headers      = {}) {
        Request req;
        req.url          = url;
        req.method       = Method::Post;
        req.body         = std::move(body);
        req.content_type = std::move(content_type);
        req.headers      = headers;
        return send(req);
    }
};

/// Construct the default libcurl-backed client. Thread-safe to call; the
/// returned client is not shared-mutable (use one per thread or guard it).
std::unique_ptr<IHttpClient> make_curl_client();

} // namespace ergo::http
