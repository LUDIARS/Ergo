#include "service_call.h"

namespace ergo::studio::plugins::studio {
namespace {

/// First `n` UTF-8 code points.
std::string clip(const std::string& s, std::size_t n) {
    std::size_t i = 0, cps = 0;
    while (i < s.size() && cps < n) {
        const auto c = static_cast<unsigned char>(s[i]);
        i += c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        ++cps;
    }
    return s.substr(0, std::min(i, s.size()));
}

} // namespace

const char* failure_name(FailureKind k) {
    switch (k) {
        case FailureKind::NotConfigured: return "not_configured";
        case FailureKind::Unreachable: return "unreachable";
        case FailureKind::Timeout: return "timeout";
        case FailureKind::Unauthorized: return "unauthorized";
        case FailureKind::Forbidden: return "forbidden";
        case FailureKind::NotFound: return "not_found";
        case FailureKind::Rejected: return "rejected";
        case FailureKind::ServerError: return "server_error";
        case FailureKind::BadResponse: return "bad_response";
    }
    return "bad_response";
}

int failure_http_status(FailureKind k) {
    switch (k) {
        case FailureKind::NotConfigured:
        case FailureKind::Unreachable: return 503;
        case FailureKind::Timeout: return 504;
        case FailureKind::Unauthorized: return 401;
        case FailureKind::Forbidden: return 403;
        case FailureKind::NotFound: return 404;
        case FailureKind::Rejected: return 400;
        case FailureKind::ServerError:
        case FailureKind::BadResponse: return 502;
    }
    return 502;
}

FailureKind classify_status(int status) {
    if (status == 401) return FailureKind::Unauthorized;
    if (status == 403) return FailureKind::Forbidden;
    if (status == 404) return FailureKind::NotFound;
    if (status >= 500) return FailureKind::ServerError;
    return FailureKind::Rejected;
}

ServiceResult ServiceResult::fail(FailureKind k, std::string msg, int status) {
    ServiceResult r;
    r.kind    = k;
    r.message = std::move(msg);
    r.status  = status;
    return r;
}

std::string summarize_error(const std::string& body) {
    JsonValue o;
    if (ergo::common::jsonm::parse(body, o) && o.is_object()) {
        for (const char* k : {"error", "message"}) {
            const JsonValue* v = o.find(k);
            if (v && v->is_string() && !v->s.empty()) return clip(v->s, 300);
        }
    }
    std::string flat;
    bool space = false;
    for (char c : body) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            space = !flat.empty();
            continue;
        }
        if (space) flat.push_back(' ');
        space = false;
        flat.push_back(c);
    }
    return clip(flat, 300);
}

ServiceCaller::ServiceCaller(std::shared_ptr<platform::IHttpClient> http, int timeout_ms)
    : http_(std::move(http)), timeout_ms_(timeout_ms) {}

ServiceResult ServiceCaller::call(const std::optional<std::string>& base, const std::string& path, const std::string& method,
                                  const JsonValue& body, const std::map<std::string, std::string>& headers,
                                  int timeout_ms) const {
    const int t = timeout_ms > 0 ? timeout_ms : timeout_ms_;
    if (!base || base->empty()) return ServiceResult::fail(FailureKind::NotConfigured, "接続先が未設定です");
    platform::HttpClientRequest req;
    req.method     = method;
    req.url        = *base + path;
    req.timeout_ms = t;
    req.headers.emplace_back("Accept", "application/json");
    for (const auto& [k, v] : headers) req.headers.emplace_back(k, v);
    if (!body.is_null()) {
        req.headers.emplace_back("Content-Type", "application/json; charset=utf-8");
        req.body = to_json(body);
    }
    const auto res = http_->send(req);
    if (!res.transport_ok) {
        if (res.timed_out) return ServiceResult::fail(FailureKind::Timeout, std::to_string(t) + "ms で応答がありません");
        return ServiceResult::fail(FailureKind::Unreachable, res.error.empty() ? std::string("接続できません") : res.error);
    }
    if (!res.ok()) return ServiceResult::fail(classify_status(res.status), summarize_error(res.body), res.status);
    ServiceResult r;
    r.ok     = true;
    r.status = res.status;
    if (res.body.find_first_not_of(" \t\r\n") == std::string::npos) return r;
    if (!ergo::common::jsonm::parse(res.body, r.data)) {
        return ServiceResult::fail(FailureKind::BadResponse, "JSON ではない応答です", res.status);
    }
    return r;
}

net::HttpResponse send_result(const std::string& service, const ServiceResult& r, const JsonValue& ok_body) {
    if (r.ok) return json_response(200, ok_body);
    return json_response(failure_http_status(r.kind),
                         jobj({{"error", jstr(failure_name(r.kind))}, {"service", jstr(service)}, {"message", jstr(r.message)}}));
}

net::HttpResponse send_error(int status, const std::string& message) {
    return json_response(status, jobj({{"error", jstr("studio")}, {"message", jstr(message)}}));
}

} // namespace ergo::studio::plugins::studio
