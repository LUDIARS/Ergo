#pragma once
/// Calls from the Studio to LUDIARS services (port of studio/net/http.ts and
/// routes/respond.ts). Results are "ok + data" or a failure kind; no
/// exception reaches the editor, and a failure body is cut to a 300-character
/// summary (ST-INV-09).
#include "../common/json_util.h"

#include "ergo/studio/platform/http_client.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace ergo::studio::plugins::studio {

enum class FailureKind {
    NotConfigured,   ///< no endpoint
    Unreachable,
    Timeout,
    Unauthorized,    ///< 401
    Forbidden,       ///< 403
    NotFound,        ///< 404
    Rejected,        ///< other 4xx
    ServerError,     ///< 5xx
    BadResponse,     ///< not JSON
};
const char* failure_name(FailureKind k);   ///< "not_configured", ...
int         failure_http_status(FailureKind k);   ///< status the Studio answers with
FailureKind classify_status(int status);

struct ServiceResult {
    bool        ok     = false;
    int         status = 0;
    JsonValue   data;          ///< null for an empty 2xx body
    FailureKind kind = FailureKind::NotConfigured;
    std::string message;

    static ServiceResult fail(FailureKind k, std::string msg, int status = 0);
};

/// The service's {error} / {message} when the body is JSON, else the
/// whitespace-collapsed text; at most 300 characters.
std::string summarize_error(const std::string& body);

class ServiceCaller {
public:
    ServiceCaller(std::shared_ptr<platform::IHttpClient> http, int timeout_ms);

    /// `base` empty = not configured. `body` null = no body (else JSON).
    /// `timeout_ms` 0 = the default.
    ServiceResult call(const std::optional<std::string>& base, const std::string& path, const std::string& method = "GET",
                       const JsonValue& body = JsonValue::make_null(),
                       const std::map<std::string, std::string>& headers = {}, int timeout_ms = 0) const;
    int           timeout_ms() const { return timeout_ms_; }

private:
    std::shared_ptr<platform::IHttpClient> http_;
    int                                    timeout_ms_;
};

/// Studio HTTP response for a result: wrap(data) on success, else
/// {error: kind, service, message} with failure_http_status().
net::HttpResponse send_result(const std::string& service, const ServiceResult& r, const JsonValue& ok_body);
/// {error: "studio", message}.
net::HttpResponse send_error(int status, const std::string& message);

} // namespace ergo::studio::plugins::studio
