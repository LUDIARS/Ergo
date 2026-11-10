#include "notification_tracker.h"

namespace ergo::studio::plugins::musa_melpomene {

JsonValue to_json_value(const NotificationState& s) {
    return jobj({{"pull_requests", to_json_array(s.pull_requests)}, {"workflow_runs", to_json_array(s.workflow_runs)},
                 {"new_review_detected", jbool(s.new_review_detected)},
                 {"workflow_status_changed", jbool(s.workflow_status_changed)}});
}

NotificationState NotificationTracker::observe(std::vector<PullRequest> prs, std::vector<WorkflowRun> runs) {
    NotificationState s;
    for (const auto& pr : prs) {
        const auto it = review_counts_.find(pr.number);
        if (it != review_counts_.end() && pr.review_count > it->second) s.new_review_detected = true;
        review_counts_[pr.number] = pr.review_count;
    }
    for (const auto& run : runs) {
        const auto it = run_statuses_.find(run.id);
        if (it != run_statuses_.end() && it->second != run.status) s.workflow_status_changed = true;
        run_statuses_[run.id] = run.status;
    }
    s.pull_requests = std::move(prs);
    s.workflow_runs = std::move(runs);
    return s;
}

} // namespace ergo::studio::plugins::musa_melpomene
