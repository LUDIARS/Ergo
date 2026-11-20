#include "lesson_protocol.h"

#include <lorica/input_budget.hpp>

#include <cmath>

namespace ergo::studio::plugins::lesson {
namespace {

constexpr std::size_t kTokenLength   = 32;
constexpr std::size_t kMaxErrorCode  = 40;
constexpr double      kMaxExactInt   = 9007199254740992.0;   // 2^53

constexpr ergo::common::jsonm::ParseLimits kReplyLimits{kMaxReplyBytes, kMaxReplyValues, kMaxReplyDepth};

bool parse_object(const std::string& body, JsonValue& out) {
    return body.size() <= kMaxReplyBytes && ergo::common::jsonm::parse(body, out, kReplyLimits) && out.is_object();
}

/// What one reply may keep: the bytes of its strings and the entries of its
/// collections (steps + features + demo states). One budget per reply.
lorica::input_budget reply_budget() {
    return lorica::input_budget(lorica::budget_limits{kMaxReplyTextSum, kMaxSteps + 2 * kMaxFeatures, 0, kMaxReplyDepth});
}

/// A string field of at most kMaxText bytes, charged to `budget`. Null when
/// missing, not a string, too long or over the budget.
const JsonValue* string_field(const JsonValue& obj, const char* key, lorica::input_budget& budget) {
    const JsonValue* v = field(obj, key);
    if (!v || !v->is_string() || v->s.size() > kMaxText || !budget.consume(v->s.size(), 0, 0)) return nullptr;
    return v;
}

/// A string that is not a field (an object key, a feature's variable name).
bool keep_text(const std::string& s, lorica::input_budget& budget) {
    return s.size() <= kMaxText && budget.consume(s.size(), 0, 0);
}

/// Strings the reply does not keep (role, op, the error code): only a length check.
const JsonValue* string_field(const JsonValue& obj, const char* key) {
    const JsonValue* v = field(obj, key);
    return v && v->is_string() && v->s.size() <= kMaxText ? v : nullptr;
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
bool parse_class_title(const JsonValue& obj, std::string& out, lorica::input_budget& budget) {
    const JsonValue* cls = field(obj, "class");
    if (!cls || !cls->is_object()) return false;
    const JsonValue* title = string_field(*cls, "title", budget);
    if (!title) return false;
    out = title->s;
    return true;
}

/// Empty on success; otherwise what was wrong.
std::string parse_check(const JsonValue& v, StepCheck& out, lorica::input_budget& budget) {
    if (!v.is_object()) return "check is not an object";
    const JsonValue* variable = string_field(v, "variable", budget);
    if (!variable || variable->s.empty()) return "check.variable is missing";
    const JsonValue* op = string_field(v, "op");
    const auto parsed_op = op ? parse_check_op(op->s) : std::nullopt;
    if (!parsed_op) return "check.op is not one of gt ge lt le eq ne";
    const JsonValue* value = field(v, "value");
    if (!value || !(value->is_number() || value->is_bool() || value->is_string())) {
        return "check.value is not a number, bool or string";
    }
    if (value->is_string() && !keep_text(value->s, budget)) return "check.value is too long";
    out.variable = variable->s;
    out.op       = *parsed_op;
    out.value    = *value;   // a scalar: nothing shared
    return {};
}

std::string parse_step(const JsonValue& v, Step& out, lorica::input_budget& budget) {
    if (!v.is_object()) return "step is not an object";
    const JsonValue* id    = string_field(v, "id", budget);
    const JsonValue* label = string_field(v, "label", budget);
    const JsonValue* done  = bool_field(v, "done");
    if (!id || !label || !done) return "step needs id, label and done (strings of at most 4096 bytes)";
    out.id    = id->s;
    out.label = label->s;
    out.done  = done->b;
    if (const JsonValue* check = field(v, "check"); check && !check->is_null()) {
        StepCheck c;
        if (std::string err = parse_check(*check, c, budget); !err.empty()) return err;
        out.check = std::move(c);
    }
    return {};
}

std::string parse_assignment(const JsonValue& v, Assignment& out, lorica::input_budget& budget) {
    if (!v.is_object()) return "assignment is neither null nor an object";
    const JsonValue* id     = string_field(v, "id", budget);
    const JsonValue* title  = string_field(v, "title", budget);
    const JsonValue* active = bool_field(v, "active");
    const JsonValue* steps  = field(v, "steps");
    if (!id || id->s.empty() || !title || !active || !steps || !steps->is_array()) {
        return "assignment needs id, title, active and steps";
    }
    if (steps->a->size() > kMaxSteps || !budget.consume(0, steps->a->size(), 0)) {
        return "assignment has more than " + std::to_string(kMaxSteps) + " steps";
    }
    out.id     = id->s;
    out.title  = title->s;
    out.active = active->b;
    for (std::size_t i = 0; i < steps->a->size(); ++i) {
        Step s;
        if (std::string err = parse_step((*steps->a)[i], s, budget); !err.empty()) {
            return "assignment.steps[" + std::to_string(i) + "]: " + err;
        }
        out.steps.push_back(std::move(s));
    }
    return {};
}

std::string parse_demo(const JsonValue& v, DemoState& out, lorica::input_budget& budget) {
    if (!v.is_object()) return "demo is not an object";
    const JsonValue* seq = field(v, "seq");
    if (!seq || !seq->is_number() || seq->n < 0 || seq->n > kMaxExactInt || std::floor(seq->n) != seq->n) {
        return "demo.seq is not a non-negative integer";
    }
    const JsonValue* states = field(v, "states");
    if (!states || !states->is_object()) return "demo.states is not an object";
    if (states->o->size() > kMaxFeatures || !budget.consume(0, states->o->size(), 0)) {
        return "demo.states has more than " + std::to_string(kMaxFeatures) + " entries";
    }
    out.seq = static_cast<uint64_t>(seq->n);
    for (const auto& [feature, on] : *states->o) {
        if (!on.is_bool()) return "demo.states has a value that is not a bool";
        if (!keep_text(feature, budget)) return "demo.states has a feature name that is too long";
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
    auto budget = reply_budget();
    const JsonValue* token = string_field(v, "token");
    if (!token || !is_token(token->s)) return bad<PairReply>("token is not 32 lowercase hex characters");
    const auto role = parse_role(v);
    if (!role) return bad<PairReply>("role is not student or presenter");
    if (!parse_class_title(v, out.class_title, budget)) return bad<PairReply>("class.title is missing or longer than 4096 bytes");
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
    auto budget = reply_budget();
    const auto role = parse_role(v);
    if (!role) return bad<LessonState>("role is not student or presenter");
    out.role = *role;
    if (!parse_class_title(v, out.class_title, budget)) return bad<LessonState>("class.title is missing or longer than 4096 bytes");

    const JsonValue* features = field(v, "features");
    if (!features || !features->is_object()) return bad<LessonState>("features is not an object");
    if (features->o->size() > kMaxFeatures || !budget.consume(0, features->o->size(), 0)) {
        return bad<LessonState>("features has more than " + std::to_string(kMaxFeatures) + " entries");
    }
    for (const auto& [feature, variable] : *features->o) {
        if (!variable.is_string() || variable.s.empty()) return bad<LessonState>("features has a value that is not a variable name");
        if (!keep_text(feature, budget) || !keep_text(variable.s, budget)) {
            return bad<LessonState>("features has a name longer than 4096 bytes, or the strings pass 256 KiB");
        }
        out.features[feature] = variable.s;
    }

    if (const JsonValue* a = field(v, "assignment"); a && !a->is_null()) {
        Assignment parsed;
        if (std::string err = parse_assignment(*a, parsed, budget); !err.empty()) return bad<LessonState>(std::move(err));
        out.assignment = std::move(parsed);
    }

    if (const JsonValue* d = field(v, "demo"); d && !d->is_null()) {
        DemoState parsed;
        if (std::string err = parse_demo(*d, parsed, budget); !err.empty()) return bad<LessonState>(std::move(err));
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
