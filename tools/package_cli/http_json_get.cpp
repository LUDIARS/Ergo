#include "http_json_get.h"

namespace ergo::package_cli {

namespace {

constexpr long kTimeoutMs = 10000;

} // namespace

HttpJson http_get_json(ergo::http::IHttpClient& client, const std::string& url) {
    ergo::http::Request req;
    req.url              = url;
    req.method           = ergo::http::Method::Get;
    req.headers          = {{"Accept", "application/json"}};
    req.timeout_ms       = kTimeoutMs;
    req.follow_redirects = false;
    const auto res = client.send(req);

    HttpJson out;
    if (!res.ok) {
        out.reason = "not reachable";
        return out;
    }
    if (res.status != 200) {
        out.reason = "HTTP " + std::to_string(res.status);
        return out;
    }
    out.ok   = true;
    out.body = res.body;
    return out;
}

} // namespace ergo::package_cli
