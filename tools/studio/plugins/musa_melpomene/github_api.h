#pragma once
/// GitHub REST calls of Melpomene (port of services/github_client.rs) over
/// an injected IHttpClient. Query values are percent-encoded (the original
/// pasted them into the URL as is). The issues list leaves out pull requests,
/// which GitHub's /issues endpoint also returns.
#include "melpomene_config.h"
#include "melpomene_models.h"

#include "ergo/studio/platform/http_client.h"

#include <memory>

namespace ergo::studio::plugins::musa_melpomene {

template <class T>
struct ApiResult {
    std::optional<T> value;
    int              status = 0;   ///< HTTP status (0 = no response)
    std::string      error;

    bool ok() const { return value.has_value(); }
};

class GitHubApi {
public:
    GitHubApi(std::shared_ptr<platform::IHttpClient> http, MelpomeneConfig cfg);

    /// "" when the API can be used, otherwise why not.
    std::string not_configured_reason() const;

    ApiResult<std::vector<Ticket>>      fetch_issues(const std::string& state, const std::vector<std::string>& labels,
                                                     unsigned page, unsigned per_page) const;
    ApiResult<Ticket>                   fetch_issue(uint64_t number) const;
    ApiResult<Ticket>                   create_issue(const std::string& title, const std::string& body,
                                                     const std::vector<std::string>& labels,
                                                     std::optional<uint64_t> milestone) const;
    ApiResult<Ticket>                   update_issue(uint64_t number, const JsonValue& patch) const;
    ApiResult<Ticket>                   close_issue(uint64_t number) const;
    ApiResult<std::vector<Comment>>     fetch_comments(uint64_t issue) const;
    ApiResult<Comment>                  post_comment(uint64_t issue, const std::string& body) const;
    ApiResult<std::vector<Milestone>>   fetch_milestones() const;
    ApiResult<Milestone>                create_milestone(const std::string& title, const std::optional<std::string>& description,
                                                         const std::optional<std::string>& due_on) const;
    ApiResult<std::vector<PullRequest>> fetch_pull_requests(const std::string& state) const;
    ApiResult<std::vector<Review>>      fetch_reviews(uint64_t pr) const;
    ApiResult<std::vector<WorkflowRun>> fetch_workflow_runs() const;
    ApiResult<std::string>              fetch_authenticated_user() const;

private:
    /// Send and parse; on a non-2xx status the body becomes the error.
    ApiResult<JsonValue> call(const std::string& method, const std::string& url, const JsonValue* body) const;

    std::shared_ptr<platform::IHttpClient> http_;
    MelpomeneConfig                        cfg_;
};

} // namespace ergo::studio::plugins::musa_melpomene
