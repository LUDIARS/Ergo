#include "github_api.h"

namespace ergo::studio::plugins::musa_melpomene {
namespace {

template <class T>
ApiResult<T> fail_from(const ApiResult<JsonValue>& r) {
    return ApiResult<T>{std::nullopt, r.status, r.error};
}

template <class T, class F>
ApiResult<std::vector<T>> list_of(const ApiResult<JsonValue>& r, const JsonValue* arr, F from) {
    if (!r.ok()) return fail_from<std::vector<T>>(r);
    std::vector<T> out;
    if (arr && arr->is_array()) {
        for (const auto& v : *arr->a) {
            if (auto x = from(v)) out.push_back(std::move(*x));
        }
    }
    return {std::move(out), r.status, {}};
}

template <class T, class F>
ApiResult<T> one_of(const ApiResult<JsonValue>& r, F from, const char* what) {
    if (!r.ok()) return fail_from<T>(r);
    if (auto x = from(*r.value)) return {std::move(*x), r.status, {}};
    return {std::nullopt, 500, std::string("Failed to parse ") + what};
}

JsonValue string_array(const std::vector<std::string>& v) {
    JsonValue a = JsonValue::make_array();
    for (const auto& s : v) a.push(jstr(s));
    return a;
}

} // namespace

GitHubApi::GitHubApi(std::shared_ptr<platform::IHttpClient> http, MelpomeneConfig cfg)
    : http_(std::move(http)), cfg_(std::move(cfg)) {}

std::string GitHubApi::not_configured_reason() const {
    if (!cfg_.is_configured()) return "Repository owner and name must be set";
    if (!is_valid_repo_part(cfg_.repository_owner) || !is_valid_repo_part(cfg_.repository_name)) {
        return "Repository owner / name contain characters GitHub does not allow";
    }
    return {};
}

ApiResult<JsonValue> GitHubApi::call(const std::string& method, const std::string& url, const JsonValue* body) const {
    ApiResult<JsonValue> out;
    if (const std::string why = not_configured_reason(); !why.empty()) {
        out.error = "Not configured: " + why;
        return out;
    }
    platform::HttpClientRequest req;
    req.method  = method;
    req.url     = url;
    req.headers = {{"User-Agent", "ars-melpomene"}, {"Accept", "application/vnd.github.v3+json"}};
    if (cfg_.github_token && !cfg_.github_token->empty()) req.headers.emplace_back("Authorization", "Bearer " + *cfg_.github_token);
    if (body) {
        req.headers.emplace_back("Content-Type", "application/json");
        req.body = to_json(*body);
    }
    const auto res = http_->send(req);
    out.status = res.status;
    if (!res.transport_ok) {
        out.error = "HTTP request failed: " + res.error;
        return out;
    }
    if (!res.ok()) {
        out.error = "API error: " + std::to_string(res.status) + " - " + res.body;
        return out;
    }
    JsonValue v;
    if (!ergo::common::jsonm::parse(res.body, v)) {
        out.error = "JSON parse error";
        return out;
    }
    out.value = std::move(v);
    return out;
}

ApiResult<std::vector<Ticket>> GitHubApi::fetch_issues(const std::string& state, const std::vector<std::string>& labels,
                                                       unsigned page, unsigned per_page) const {
    std::string url = cfg_.api_base_url() + "/issues?page=" + std::to_string(page) + "&per_page=" + std::to_string(per_page);
    if (!state.empty()) url += "&state=" + platform::url_encode(state);
    if (!labels.empty()) {
        std::string joined;
        for (const auto& l : labels) joined += (joined.empty() ? "" : ",") + l;
        url += "&labels=" + platform::url_encode(joined);
    }
    const auto r = call("GET", url, nullptr);
    return list_of<Ticket>(r, r.ok() ? &*r.value : nullptr, [](const JsonValue& v) -> std::optional<Ticket> {
        if (field(v, "pull_request")) return std::nullopt;
        return ticket_from_github(v);
    });
}

ApiResult<Ticket> GitHubApi::fetch_issue(uint64_t number) const {
    return one_of<Ticket>(call("GET", cfg_.api_base_url() + "/issues/" + std::to_string(number), nullptr),
                          ticket_from_github, "issue");
}

ApiResult<Ticket> GitHubApi::create_issue(const std::string& title, const std::string& body,
                                          const std::vector<std::string>& labels, std::optional<uint64_t> milestone) const {
    JsonValue payload = jobj({{"title", jstr(title)}, {"body", jstr(body)}, {"labels", string_array(labels)}});
    if (milestone) payload.set("milestone", jnum(static_cast<double>(*milestone)));
    return one_of<Ticket>(call("POST", cfg_.api_base_url() + "/issues", &payload), ticket_from_github, "created issue");
}

ApiResult<Ticket> GitHubApi::update_issue(uint64_t number, const JsonValue& patch) const {
    // Only the fields the original passed through.
    JsonValue payload = JsonValue::make_object();
    for (const char* k : {"title", "body", "state", "labels"}) {
        if (const JsonValue* v = field(patch, k)) payload.set(k, deep_copy(*v));
    }
    return one_of<Ticket>(call("PATCH", cfg_.api_base_url() + "/issues/" + std::to_string(number), &payload),
                          ticket_from_github, "updated issue");
}

ApiResult<Ticket> GitHubApi::close_issue(uint64_t number) const { return update_issue(number, jobj({{"state", jstr("closed")}})); }

ApiResult<std::vector<Comment>> GitHubApi::fetch_comments(uint64_t issue) const {
    const auto r = call("GET", cfg_.api_base_url() + "/issues/" + std::to_string(issue) + "/comments", nullptr);
    return list_of<Comment>(r, r.ok() ? &*r.value : nullptr, comment_from_github);
}

ApiResult<Comment> GitHubApi::post_comment(uint64_t issue, const std::string& body) const {
    const JsonValue payload = jobj({{"body", jstr(body)}});
    return one_of<Comment>(call("POST", cfg_.api_base_url() + "/issues/" + std::to_string(issue) + "/comments", &payload),
                           comment_from_github, "comment");
}

ApiResult<std::vector<Milestone>> GitHubApi::fetch_milestones() const {
    const auto r = call("GET", cfg_.api_base_url() + "/milestones?state=all", nullptr);
    return list_of<Milestone>(r, r.ok() ? &*r.value : nullptr, milestone_from_github);
}

ApiResult<Milestone> GitHubApi::create_milestone(const std::string& title, const std::optional<std::string>& description,
                                                 const std::optional<std::string>& due_on) const {
    JsonValue payload = jobj({{"title", jstr(title)}});
    if (description) payload.set("description", jstr(*description));
    if (due_on) payload.set("due_on", jstr(*due_on));
    return one_of<Milestone>(call("POST", cfg_.api_base_url() + "/milestones", &payload), milestone_from_github, "milestone");
}

ApiResult<std::vector<PullRequest>> GitHubApi::fetch_pull_requests(const std::string& state) const {
    const auto r = call("GET", cfg_.api_base_url() + "/pulls?state=" + platform::url_encode(state.empty() ? "open" : state),
                        nullptr);
    return list_of<PullRequest>(r, r.ok() ? &*r.value : nullptr, pull_request_from_github);
}

ApiResult<std::vector<Review>> GitHubApi::fetch_reviews(uint64_t pr) const {
    const auto r = call("GET", cfg_.api_base_url() + "/pulls/" + std::to_string(pr) + "/reviews", nullptr);
    return list_of<Review>(r, r.ok() ? &*r.value : nullptr, review_from_github);
}

ApiResult<std::vector<WorkflowRun>> GitHubApi::fetch_workflow_runs() const {
    const auto r = call("GET", cfg_.api_base_url() + "/actions/runs?per_page=10", nullptr);
    return list_of<WorkflowRun>(r, r.ok() ? field(*r.value, "workflow_runs") : nullptr, workflow_run_from_github);
}

ApiResult<std::string> GitHubApi::fetch_authenticated_user() const {
    const auto r = call("GET", "https://api.github.com/user", nullptr);
    if (!r.ok()) return fail_from<std::string>(r);
    const JsonValue* login = field(*r.value, "login");
    if (!login || !login->is_string()) return {std::nullopt, 500, "Failed to get user login"};
    return {login->s, r.status, {}};
}

} // namespace ergo::studio::plugins::musa_melpomene
