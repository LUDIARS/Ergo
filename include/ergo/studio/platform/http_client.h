#pragma once
/// Outgoing HTTP(S) for plugins (GitHub API, bridges to local tools).
/// The system implementation is WinHTTP on Windows (TLS, the system proxy and
/// certificate store) and the managed libcurl elsewhere when the build has it
/// (-DERGO_WITH_CURL=ON: TLS from its backend with verification on, proxy
/// from the environment). A build without one returns an explicit "not
/// available in this build" error on every send.
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ergo::studio::platform {

struct HttpClientRequest {
    std::string                                      method = "GET";
    std::string                                      url;       ///< absolute http:// or https://
    std::vector<std::pair<std::string, std::string>> headers;
    std::string                                      body;
    int                                              timeout_ms = 30000;
};

struct HttpClientResponse {
    bool        transport_ok = false;   ///< false: no HTTP response at all (see error)
    bool        timed_out    = false;   ///< no response within timeout_ms
    int         status       = 0;
    std::string body;
    std::string error;

    bool ok() const { return transport_ok && status >= 200 && status < 300; }
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    /// Blocking; safe to call from several threads.
    virtual HttpClientResponse send(const HttpClientRequest& req) = 0;
};

std::shared_ptr<IHttpClient> make_system_http_client();

/// RFC 3986 percent-encoding of a query / path component (UTF-8 in).
std::string url_encode(const std::string& s);

} // namespace ergo::studio::platform
