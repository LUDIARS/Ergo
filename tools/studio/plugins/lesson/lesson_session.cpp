#include "lesson_session.h"

namespace ergo::studio::plugins::lesson {

bool operator==(const SessionView& a, const SessionView& b) {
    auto same_assignment = [](const std::optional<AssignmentRows>& x, const std::optional<AssignmentRows>& y) {
        if (x.has_value() != y.has_value()) return false;
        if (!x) return true;
        if (x->title != y->title || x->active != y->active || x->steps.size() != y->steps.size()) return false;
        for (std::size_t i = 0; i < x->steps.size(); ++i) {
            const StepRow& s = x->steps[i];
            const StepRow& t = y->steps[i];
            if (s.label != t.label || s.done != t.done || s.automatic != t.automatic) return false;
        }
        return true;
    };
    return a.linked == b.linked && a.role == b.role && a.host == b.host && a.class_title == b.class_title &&
           a.end == b.end && a.failed_call == b.failed_call && a.failure == b.failure &&
           same_assignment(a.assignment, b.assignment) && a.demo == b.demo;
}

LessonSession::LessonSession(LessonClient client, std::shared_ptr<IGameVariables> variables)
    : client_(std::move(client)), variables_(std::move(variables)) {}

void LessonSession::drop(LinkEnd why) {
    origin_.clear();
    host_.clear();
    token_.clear();
    role_ = Role::None;
    class_title_.clear();
    assignment_.reset();
    tracked_id_.clear();
    satisfied_.clear();
    reported_.clear();
    assignment_ended_ = false;
    demo_.reset();
    failed_call_ = LessonCall::None;
    failure_     = {};
    end_         = why;
}

void LessonSession::fail(LessonCall call, CallFailure f) {
    failed_call_ = call;
    failure_     = std::move(f);
}

bool LessonSession::ended_by(const CallFailure& f) {
    if (!f.is_http(401, "tool_unlinked")) return false;
    drop(LinkEnd::Unlinked);
    return true;
}

void LessonSession::link(const LessonLink& link) {
    drop(LinkEnd::None);
    auto r = client_.pair(link);
    if (!r.ok()) {
        fail(LessonCall::Pair, std::move(r.failure));
        return;
    }
    origin_      = link.origin;
    host_        = link.host;
    token_       = std::move(r.value->token);
    role_        = r.value->role;
    class_title_ = std::move(r.value->class_title);
    poll_ms_     = r.value->poll_ms;
}

void LessonSession::disconnect() { drop(LinkEnd::Disconnected); }

void LessonSession::poll() {
    if (!linked()) return;
    auto r = client_.state(origin_, token_);
    if (!r.ok()) {
        if (!ended_by(r.failure)) fail(LessonCall::State, std::move(r.failure));
        return;   // what was shown stays; the next poll tries again
    }
    failed_call_ = LessonCall::None;
    failure_     = {};
    role_        = r.value->role;
    class_title_ = std::move(r.value->class_title);
    assignment_  = std::move(r.value->assignment);

    if (role_ == Role::Student) {
        demo_.reset();
        report_progress();
    } else if (role_ == Role::Presenter) {
        demo_.observe(r.value->demo->seq, r.value->demo->states, r.value->features);   // parse_state guarantees demo
        demo_.apply(*variables_);
    }
}

void LessonSession::report_progress() {
    if (!assignment_) return;
    const Assignment& a = *assignment_;
    if (a.id != tracked_id_) {
        tracked_id_ = a.id;
        satisfied_.clear();
        reported_.clear();
        assignment_ended_ = false;
    }
    if (!a.active || assignment_ended_) return;

    for (std::size_t i = 0; i < a.steps.size(); ++i) {
        const Step& step = a.steps[i];
        if (!step.check || step.done || reported_.count(i)) continue;
        if (!satisfied_.count(i)) {
            if (!check_satisfied(*step.check, variables_->find(step.check->variable))) continue;
            satisfied_.insert(i);   // latched: reported even if the value changes back before the POST gets through
        }
        CallFailure f = client_.progress(origin_, token_, a.id, i);
        if (!f.failed()) {
            reported_.insert(i);
            continue;
        }
        if (ended_by(f)) return;
        if (f.is_http(409, "not_assigned")) {
            assignment_ended_ = true;   // the assignment is over: not an error, just stop
            return;
        }
        fail(LessonCall::Progress, std::move(f));
        return;   // the remaining steps wait for the next poll (no point in queueing timeouts)
    }
}

SessionView LessonSession::view() const {
    SessionView v;
    v.linked      = linked();
    v.role        = role_;
    v.host        = host_;
    v.class_title = class_title_;
    v.end         = end_;
    v.failed_call = failed_call_;
    v.failure     = failure_;
    if (role_ == Role::Student && assignment_) {
        AssignmentRows rows;
        rows.title  = assignment_->title;
        rows.active = assignment_->active;
        const bool tracked = assignment_->id == tracked_id_;
        for (std::size_t i = 0; i < assignment_->steps.size(); ++i) {
            const Step& s = assignment_->steps[i];
            rows.steps.push_back({s.label, s.done || (tracked && reported_.count(i) > 0), s.check.has_value()});
        }
        v.assignment = std::move(rows);
    }
    if (role_ == Role::Presenter) v.demo = demo_.entries();
    return v;
}

} // namespace ergo::studio::plugins::lesson
