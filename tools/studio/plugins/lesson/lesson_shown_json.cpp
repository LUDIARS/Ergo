#include "lesson_shown_json.h"

namespace ergo::studio::plugins::lesson {
namespace {

const char* end_id(LinkEnd e) {
    switch (e) {
    case LinkEnd::Disconnected: return "disconnected";
    case LinkEnd::Unlinked: return "unlinked";
    case LinkEnd::None: break;
    }
    return "none";
}

const char* call_id(LessonCall c) {
    switch (c) {
    case LessonCall::Pair: return "pair";
    case LessonCall::State: return "state";
    case LessonCall::Progress: return "progress";
    case LessonCall::None: break;
    }
    return "none";
}

const char* kind_id(FailureKind k) {
    switch (k) {
    case FailureKind::Transport: return "transport";
    case FailureKind::Timeout: return "timeout";
    case FailureKind::Http: return "http";
    case FailureKind::Malformed: return "malformed";
    case FailureKind::None: break;
    }
    return "none";
}

JsonValue failure_json(const SessionView& s) {
    if (!s.failure.failed()) return JsonValue::make_null();
    return jobj({{"call", jstr(call_id(s.failed_call))},
                 {"kind", jstr(kind_id(s.failure.kind))},
                 {"status", jnum(s.failure.status)},
                 {"code", jstr(s.failure.code)},
                 {"detail", jstr(s.failure.detail)}});
}

JsonValue assignment_json(const std::optional<AssignmentRows>& a) {
    if (!a) return JsonValue::make_null();
    JsonValue steps = JsonValue::make_array();
    for (const auto& step : a->steps) {
        steps.push(jobj({{"label", jstr(step.label)}, {"done", jbool(step.done)}, {"automatic", jbool(step.automatic)}}));
    }
    return jobj({{"title", jstr(a->title)}, {"active", jbool(a->active)}, {"steps", steps}});
}

JsonValue demo_json(const std::vector<DemoEntry>& demo) {
    JsonValue out = JsonValue::make_array();
    for (const auto& e : demo) {
        out.push(jobj({{"feature", jstr(e.feature)},
                       {"variable", jstr(e.variable)},
                       {"on", e.on ? jbool(*e.on) : JsonValue::make_null()},
                       {"status", jstr(demo_status_id(e.status))}}));
    }
    return out;
}

} // namespace

const char* demo_status_id(DemoStatus s) {
    switch (s) {
    case DemoStatus::NotCued: return "not_cued";
    case DemoStatus::Applied: return "applied";
    case DemoStatus::Requested: return "requested";
    case DemoStatus::NotFollowing: return "not_following";
    case DemoStatus::WaitingForGame: return "waiting_for_game";
    case DemoStatus::NotBool: return "not_bool";
    case DemoStatus::NoVariable: return "no_variable";
    }
    return "";
}

JsonValue lesson_shown_json(const LessonShown& shown) {
    const SessionView& s = shown.session;
    return jobj({{"connecting", jbool(shown.connecting)},
                 {"link_error", jstr(shown.link_error)},
                 {"linked", jbool(s.linked)},
                 {"role", jstr(role_name(s.linked ? s.role : Role::None))},
                 {"host", jstr(s.host)},
                 {"class", jstr(s.class_title)},
                 {"end", jstr(end_id(s.end))},
                 {"failure", failure_json(s)},
                 {"assignment", assignment_json(s.role == Role::Student ? s.assignment : std::nullopt)},
                 {"demo", demo_json(s.role == Role::Presenter ? s.demo : std::vector<DemoEntry>{})}});
}

} // namespace ergo::studio::plugins::lesson
