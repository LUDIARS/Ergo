#include "lesson_client.h"

namespace ergo::studio::plugins::lesson {
namespace {

const char* const kJson = "application/json";

void authorize(platform::HttpClientRequest& req, const std::string& token) {
    req.headers.emplace_back("Authorization", "Bearer " + token);
}

} // namespace

LessonClient::LessonClient(std::shared_ptr<platform::IHttpClient> http, int timeout_ms)
    : http_(std::move(http)), timeout_ms_(timeout_ms) {}

CallResult<PairReply> LessonClient::pair(const LessonLink& link) const {
    platform::HttpClientRequest req;
    req.method     = "POST";
    req.url        = link.origin + "/v1/tool/pair";
    req.timeout_ms = timeout_ms_;
    req.headers.emplace_back("Content-Type", kJson);
    req.body = pair_request_body(link.code);
    const auto res = http_->send(req);
    if (CallFailure f = classify_response(res); f.failed()) return {std::nullopt, std::move(f)};
    auto parsed = parse_pair_reply(res.body);
    if (!parsed.value) return {std::nullopt, malformed(std::move(parsed.error))};
    return {std::move(parsed.value), {}};
}

CallResult<LessonState> LessonClient::state(const std::string& origin, const std::string& token) const {
    platform::HttpClientRequest req;
    req.method     = "GET";
    req.url        = origin + "/v1/tool/state";
    req.timeout_ms = timeout_ms_;
    authorize(req, token);
    const auto res = http_->send(req);
    if (CallFailure f = classify_response(res); f.failed()) return {std::nullopt, std::move(f)};
    auto parsed = parse_state(res.body);
    if (!parsed.value) return {std::nullopt, malformed(std::move(parsed.error))};
    return {std::move(parsed.value), {}};
}

CallFailure LessonClient::progress(const std::string& origin, const std::string& token, const std::string& assignment_id,
                                   std::size_t step) const {
    platform::HttpClientRequest req;
    req.method     = "POST";
    req.url        = origin + "/v1/tool/progress";
    req.timeout_ms = timeout_ms_;
    authorize(req, token);
    req.headers.emplace_back("Content-Type", kJson);
    req.body = progress_request_body(assignment_id, step);
    const auto res = http_->send(req);
    if (CallFailure f = classify_response(res); f.failed()) return f;
    if (std::string err = parse_progress_ack(res.body); !err.empty()) return malformed(std::move(err));
    return {};
}

} // namespace ergo::studio::plugins::lesson
