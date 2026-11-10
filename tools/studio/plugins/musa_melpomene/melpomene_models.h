#pragma once
/// Melpomene records (port of Ars-Musa melpomene models/ticket.rs, github.rs,
/// milestone.rs): conversion from GitHub REST JSON and to the wire JSON the
/// Studio API returns (field names as in the Rust structs).
#include "../common/json_util.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::musa_melpomene {

enum class Priority { Low, Medium, High, Critical };
enum class Category { Bug, Feature, Improvement, Question };
const char* priority_name(Priority p);   ///< "Low" ... (as Rust {:?} prints it)
const char* category_name(Category c);   ///< "Bug" ...
Priority    parse_priority(const std::string& s);   ///< case-insensitive, Medium otherwise
Category    parse_category(const std::string& s);   ///< case-insensitive, Bug otherwise

struct Comment {
    uint64_t    id = 0;
    std::string body, user, created_at, updated_at;
};

struct Ticket {
    std::string                          id;   ///< "github-<number>"
    std::optional<uint64_t>              github_issue_number;
    std::string                          title;
    std::string                          description;
    Priority                             priority = Priority::Medium;
    Category                             category = Category::Bug;
    std::vector<std::string>             labels;
    std::optional<uint64_t>              milestone_id;
    std::optional<std::string>           scene_name;
    std::optional<std::string>           object_path;
    std::optional<std::array<double, 3>> world_position;
    std::optional<std::array<double, 2>> screen_position;
    std::string                          created_at, updated_at;
    std::string                          state = "open";
    std::optional<std::string>           author;
    std::vector<std::string>             assignees;
    std::vector<Comment>                 comments;

    /// Issue body: description, then a "---" block with Scene / Object Path /
    /// World Position / Priority / Category lines (parsed back on read).
    std::string to_github_body() const;
};

struct Milestone {
    uint64_t                   number = 0;
    std::string                title;
    std::optional<std::string> description;
    std::string                state = "open";
    uint64_t                   open_issues = 0, closed_issues = 0;
    std::optional<std::string> due_on;
    std::string                created_at, updated_at;

    double progress_percent() const;
};

struct Review {
    uint64_t                   id = 0;
    std::string                state;
    std::optional<std::string> body;
    std::string                user, submitted_at;
};

struct PullRequest {
    uint64_t            number = 0;
    std::string         title, state = "open", html_url, head_ref, base_ref;
    std::optional<bool> mergeable;
    std::string         user, created_at, updated_at;
    uint64_t            review_count = 0;
    std::vector<Review> reviews;
};

struct WorkflowRun {
    uint64_t                   id = 0;
    std::string                name, status;
    std::optional<std::string> conclusion;
    std::string                html_url, head_branch, created_at, updated_at;
};

/// nullopt when a required field (number / id, title / name) is missing.
std::optional<Ticket>      ticket_from_github(const JsonValue& issue);
std::optional<Comment>     comment_from_github(const JsonValue& v);
std::optional<Milestone>   milestone_from_github(const JsonValue& v);
std::optional<PullRequest> pull_request_from_github(const JsonValue& v);
std::optional<Review>      review_from_github(const JsonValue& v);
std::optional<WorkflowRun> workflow_run_from_github(const JsonValue& v);

JsonValue to_json_value(const Ticket& t);
JsonValue to_json_value(const Comment& c);
JsonValue to_json_value(const Milestone& m);
JsonValue to_json_value(const Review& r);
JsonValue to_json_value(const PullRequest& p);
JsonValue to_json_value(const WorkflowRun& w);

template <class T>
JsonValue to_json_array(const std::vector<T>& v) {
    JsonValue a = JsonValue::make_array();
    for (const auto& x : v) a.push(to_json_value(x));
    return a;
}

} // namespace ergo::studio::plugins::musa_melpomene
