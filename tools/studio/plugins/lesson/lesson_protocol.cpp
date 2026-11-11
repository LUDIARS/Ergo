#include "lesson_protocol.h"

#include <cmath>

namespace ergo::studio::plugins::lesson {
namespace {

constexpr std::size_t kTokenLength   = 32;
constexpr std::size_t kMaxErrorCode  = 40;
constexpr double      kMaxExactInt   = 9007199254740992.0;   // 2^53

bool parse_object(const std::string& body, JsonValue& out) {
    return ergo::common::jsonm::parse(body, out) && out.is_object();
}

const JsonValue* string_field(const JsonValue& obj, const char* key) {
    const JsonValue* v = field(obj, key);
    return v && v->is_string() ? v : nullptr;
}

const JsonValue* bool_field(const JsonValue& obj, const char* key) {
    const JsonValue* v = field(obj, key);
    return v && v->is_bool() ? v : nullptr;
}

bool is_token(const std::string& t) {
    if (t.size() != kTokenLength) return false;
    for (char c : t) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

/// A server error code is shown to the user, so only a plain identifier is kept.
bool is_error_code(const std::string& s) {
    if (s.empty() || s.size() > kMaxErrorCode) return false;
    for (char c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

std::optional<Role> parse_role(const JsonValue& obj) {
    const JsonValue* r = string_field(obj, "role");
    if (!r) return std::nullopt;
    if (r->s == "student") return Role::Student;
    if (r->s == "presenter") return Role::Presenter;
    return std::nullopt;
}

/// class.title; false when it is missing.
bool parse_class_title(const JsonValue& obj, std::string& out) {
    const JsonValue* cls = field(obj, "class");
    if (!cls || !cls->is_object()) return false;
    const JsonValue* title = string_field(*cls, "title");
    if (!title) return false;
    out = title->s;
    return true;
}

/// Empty on success; otherwise what was wrong.
std::string parse_check(const JsonValue& v, StepCheck& out) {
    if (!v.is_object()) return "check is not an object";
    const JsonValue* variable = string_field(v, "variable");
    if (!variable || variable->s.empty()) return "check.variable is missing";
    const JsonValue* op = string_field(v, "op");
    const auto parsed_op = op ? parse_check_op(op->s) : std::nullopt;
    if (!parsed_op) return "check.op is not one of gt ge lt le eq ne";
    const JsonValue* value = field(v, "value");
    if (!value || !(value->is_number() || value->is_bool() || value->is_string())) {
        return "check.value is not a number, bool or string";
    }
    out.variable = variable->s;
    out.op       = *parsed_op;
    out.value    = *value;   // a scalar: nothing shared
    return {};
}

std::string parse_step(const JsonValue& v, Step& out) {
    if (!v.is_object()) return "step is not an object";
    const JsonValue* id    = string_field(v, "id");
    const JsonValue* label = string_field(v, "label");
    const JsonValue* done  = bool_field(v, "done");
    if (!id || !label || !done) return "step needs id, label and done";
    out.id    = id->s;
    out.label = label->s;
    out.done  = done->b;
    if (const JsonValue* check = field(v, "check"); check && !check->is_null()) {
        StepCheck c;
        if (std::string err = parse_check(*check, c); !err.empty()) return err;
        out.check = std::move(c);
    }
    return {};
}

std::string parse_assignment(const JsonValue& v, Assignment& out) {
    if (!v.is_object()) return "assignment is neither null nor an object";
    const JsonValue* id     = string_field(v, "id");
    const JsonValue* title  = string_field(v, "title");
    const JsonValue* active = bool_field(v, "active");
    const JsonValue* steps  = field(v, "steps");
    if (!id || id->s.empty() || !title || !active || !steps || !steps->is_array()) {
        return "assignment needs id, title, active and steps";
    }
    out.id     = id->s;
    out.title  = title->s;
    out.active = active->b;
    for (std::size_t i = 0; i < steps->a->size(); ++i) {
        Step s;
        if (std::string err = parse_step((*steps->a)[i], s); !err.empty()) {
            return "assignment.steps[" + std::to_string(i) + "]: " + err;
        }
        out.steps.push_back(std::move(s));
    }
    return {};
}

std::string parse_demo(const JsonValue& v, DemoState& out) {
    if (!v.is_object()) return "demo is not an object";
    const JsonValue* seq = field(v, "seq");
    if (!seq || !seq->is_number() || seq->n < 0 || seq->n > kMaxExactInt || std::floor(seq->n) != seq->n) {
        return "demo.seq is not a non-negative integer";
    }
    const JsonValue* states = field(v, "states");
    if (!states || !states->is_object()) return "demo.states is not an object";
    out.seq = static_cast<uint64_t>(seq->n);
    for (const auto& [feature, on] : *states->o) {
        if (!on.is_bool()) return "demo.states has a value that is not a bool";
        out.states[feature] = on.b;
    }
    return {};
}

template <class T>
Parsed<T> bad(std::string what) {
    return {std::nullopt, std::move(what)};
}

} // namespace

const char* role_name(Role r) {
    switch (r) {
    case Role::Student: return "student";
    case Role::Presenter: return "presenter";
    case Role::None: break;
    }
    return "none";
}

int clamp_poll_ms(double poll_ms) {
    if (!(poll_ms >= kMinPollMs)) return kMinPollMs;   // also NaN
    if (poll_ms > kMaxPollMs) return kMaxPollMs;
    return static_cast<int>(poll_ms);
}

Parsed<PairReply> parse_pair_reply(const std::string& body) {
    JsonValue v;
    if (!parse_object(body, v)) return bad<PairReply>("the body is not a JSON object");
    PairReply out;
    const JsonValue* token = string_field(v, "token");
    if (!token || !is_token(token->s)) return bad<PairReply>("token is not 32 lowercase hex characters");
    const auto role = parse_role(v);
    if (!role) return bad<PairReply>("role is not student or presenter");
    if (!parse_class_title(v, out.class_title)) return bad<PairReply>("class.title is missing");
    const JsonValue* poll = field(v, "poll_ms");
    if (!poll || !poll->is_number()) return bad<PairReply>("poll_ms is missing");
    out.token   = token->s;
    out.role    = *role;
    out.poll_ms = clamp_poll_ms(poll->n);
    return {std::move(out), {}};
}

Parsed<LessonState> parse_state(const std::string& body) {
    JsonValue v;
    if (!parse_object(body, v)) return bad<LessonState>("the body is not a JSON object");
    LessonState out;
    const auto role = parse_role(v);
    if (!role) return bad<LessonState>("role is not student or presenter");
    out.role = *role;
    if (!parse_class_title(v, out.class_title)) return bad<LessonState>("class.title is missing");

    const JsonValue* features = field(v, "features");
    if (!features || !features->is_object()) return bad<LessonState>("features is not an object");
    for (const auto& [feature, variable] : *features->o) {
        if (!variable.is_string() || variable.s.empty()) return bad<LessonState>("features has a value that is not a variable name");
        out.features[feature] = variable.s;
    }

    if (const JsonValue* a = field(v, "assignment"); a && !a->is_null()) {
        Assignment parsed;
        if (std::string err = parse_assignment(*a, parsed); !err.empty()) return bad<LessonState>(std::move(err));
        out.assignment = std::move(parsed);
    }

    if (const JsonValue* d = field(v, "demo"); d && !d->is_null()) {
        DemoState parsed;
        if (std::string err = parse_demo(*d, parsed); !err.empty()) return bad<LessonState>(std::move(err));
        out.demo = std::move(parsed);
    }
    if (out.role == Role::Presenter && !out.demo) return bad<LessonState>("demo is missing for a presenter");
    return {std::move(out), {}};
}

std::string parse_progress_ack(const std::string& body) {
    JsonValue v;
    if (!parse_object(body, v)) return "the body is not a JSON object";
    const JsonValue* ok = bool_field(v, "ok");
    if (!ok || !ok->b) return "ok is not true";
    return {};
}

std::string pair_request_body(const std::string& code) {
    return to_json(jobj({{"link", jstr(code)}, {"tool", jstr("ergo-studio")}}));
}

std::string progress_request_body(const std::string& assignment_id, std::size_t step) {
    return to_json(jobj({{"assignment_id", jstr(assignment_id)},
                         {"step", jnum(static_cast<double>(step))},
                         {"done", jbool(true)}}));
}

bool operator==(const CallFailure& a, const CallFailure& b) {
    return a.kind == b.kind && a.status == b.status && a.code == b.code && a.detail == b.detail;
}

CallFailure classify_response(const platform::HttpClientResponse& res) {
    CallFailure f;
    if (!res.transport_ok) {
        f.kind   = res.timed_out ? FailureKind::Timeout : FailureKind::Transport;
        f.detail = res.error;
        return f;
    }
    if (res.ok()) return f;
    f.kind   = FailureKind::Http;
    f.status = res.status;
    JsonValue body;
    if (parse_object(res.body, body)) {
        const JsonValue* code = string_field(body, "error");
        if (code && is_error_code(code->s)) f.code = code->s;
    }
    return f;
}

CallFailure malformed(std::string what) {
    CallFailure f;
    f.kind   = FailureKind::Malformed;
    f.detail = std::move(what);
    return f;
}

} // namespace ergo::studio::plugins::lesson
