#pragma once
/// Change detection between notification polls (port of
/// services/notification.rs): a PR whose review count grew, a workflow run
/// whose status changed. Pure: the caller fetches the data.
#include "melpomene_models.h"

#include <map>

namespace ergo::studio::plugins::musa_melpomene {

struct NotificationState {
    std::vector<PullRequest> pull_requests;
    std::vector<WorkflowRun> workflow_runs;
    bool                     new_review_detected     = false;
    bool                     workflow_status_changed = false;
};
JsonValue to_json_value(const NotificationState& s);

class NotificationTracker {
public:
    /// Compare with the previous observation and remember this one. Items
    /// seen for the first time never count as changed.
    NotificationState observe(std::vector<PullRequest> prs, std::vector<WorkflowRun> runs);

private:
    std::map<uint64_t, uint64_t>    review_counts_;
    std::map<uint64_t, std::string> run_statuses_;
};

} // namespace ergo::studio::plugins::musa_melpomene
