#include "fake_http.h"
#include "fake_ws.h"
#include "lesson_variables.h"
#include "plugins/lesson/lesson_plugin.h"
#include "plugins/variable/variable_plugin.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::lesson;
using namespace studio_test;

namespace {

const char* const kOrigin = "http://127.0.0.1:4410";
const char* const kLink   = "http://127.0.0.1:4410/v1/tool#7K2M9QX4TB";
const char* const kToken  = "0123456789abcdef0123456789abcdef";
const std::string kPairUrl     = std::string("POST ") + kOrigin + "/v1/tool/pair";
const std::string kStateUrl    = std::string("GET ") + kOrigin + "/v1/tool/state";
const std::string kProgressUrl = std::string("POST ") + kOrigin + "/v1/tool/progress";

std::string pair_body(const char* role) {
    return std::string(R"({"token":")") + kToken + R"(","role":")" + role + R"(","class":{"title":"Action"},"poll_ms":2000})";
}

/// Steps: 0 = no check, 1 = hit stop > 0, 2 = a check the server already has as done.
const char* const kStudentState = R"({
  "role":"student","class":{"title":"Action"},
  "features":{"hit_stop":"feel.hit_stop"},
  "assignment":{"id":"restore-feel","title":"Restore the feel","active":true,"steps":[
    {"id":"explain","label":"Explain","done":false},
    {"id":"add-hitstop","label":"Add hit stop","done":false,"check":{"variable":"feel.hit_stop_last_seconds","op":"gt","value":0}},
    {"id":"shake","label":"Shake","done":true,"check":{"variable":"feel.camera_shake","op":"eq","value":true}}
  ]}
})";

std::string presenter_state(int seq, const char* states) {
    return std::string(R"({"role":"presenter","class":{"title":"Action"},)") +
           R"("features":{"hit_stop":"feel.hit_stop","knockback":"feel.knockback"},"assignment":null,)" +
           R"("demo":{"seq":)" + std::to_string(seq) + R"(,"states":)" + states + "}}";
}

class FakeVars final : public IGameVariables {
public:
    std::map<std::string, GameVariable>            vars;
    std::vector<std::pair<std::string, JsonValue>> sets;

    std::optional<GameVariable> find(const std::string& name) const override {
        const auto it = vars.find(name);
        if (it == vars.end()) return std::nullopt;
        return it->second;
    }
    bool request_set(const std::string& name, const JsonValue& value) override {
        if (!vars.count(name)) return false;
        sets.emplace_back(name, value);
        return true;
    }
};

GameVariable var(const char* kind, JsonValue value) { return {kind, std::move(value)}; }

std::size_t count(const FakeHttp& http, const std::string& method_url) {
    std::size_t n = 0;
    for (const auto& r : http.seen) {
        if (r.method + " " + r.url == method_url) ++n;
    }
    return n;
}

/// A session paired through FakeHttp as `role`.
struct Linked {
    explicit Linked(const char* role) : http(std::make_shared<FakeHttp>()), vars(std::make_shared<FakeVars>()),
                                         session(LessonClient(http), vars) {
        http->routes[kPairUrl] = {200, pair_body(role)};
        const auto link = parse_lesson_link(kLink);
        session.link(*link.link);
    }
    std::shared_ptr<FakeHttp> http;
    std::shared_ptr<FakeVars> vars;
    LessonSession             session;
};

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

} // namespace

// ---- connection string --------------------------------------------------------------

TEST(StudioLessonLink, AcceptsTheContractForms) {
    struct Case {
        const char* text;
        const char* origin;
        const char* host;
        const char* code;
    };
    const Case cases[] = {
        {"https://lesson.example.org/v1/tool#7K2M9QX4TB", "https://lesson.example.org", "lesson.example.org", "7K2M9QX4TB"},
        {"http://127.0.0.1:4410/v1/tool#7K2M9QX4TB", "http://127.0.0.1:4410", "127.0.0.1", "7K2M9QX4TB"},
        {"  https://lesson.example.org/v1/tool#7k2m9qx4tb\r\n", "https://lesson.example.org", "lesson.example.org", "7K2M9QX4TB"},
        {"HTTPS://Lesson.Example.ORG:8443/v1/tool#0123456789", "https://lesson.example.org:8443", "lesson.example.org", "0123456789"},
        {"http://[::1]:4410/v1/tool#ABCDEFGHJK", "http://[::1]:4410", "::1", "ABCDEFGHJK"},
        {"http://localhost/v1/tool#MNPQRSTVWX", "http://localhost", "localhost", "MNPQRSTVWX"},
    };
    for (const auto& c : cases) {
        const auto r = parse_lesson_link(c.text);
        ASSERT_TRUE(r.link.has_value());
        EXPECT_TRUE(r.error.empty());
        EXPECT_EQ(r.link->origin, std::string(c.origin));
        EXPECT_EQ(r.link->host, std::string(c.host));
        EXPECT_EQ(r.link->code, std::string(c.code));
    }
}

TEST(StudioLessonLink, RejectsEverythingElseWithAReasonThatDoesNotEchoTheInput) {
    const char* const cases[] = {
        "",
        "   ",
        "7K2M9QX4TB",
        "ftp://lesson.example.org/v1/tool#7K2M9QX4TB",
        "lesson.example.org/v1/tool#7K2M9QX4TB",
        "https://lesson.example.org/v1/tool",                       // no code
        "https://lesson.example.org/v1/tool#",                      // empty code
        "https://lesson.example.org/v1/tool#7K2M9QX4T",             // 9 symbols
        "https://lesson.example.org/v1/tool#7K2M9QX4TBZ",           // 11 symbols
        "https://lesson.example.org/v1/tool#7K2M9QX4TI",            // I is not Crockford
        "https://lesson.example.org/v1/tool#7K2M9QX4TL",
        "https://lesson.example.org/v1/tool#7K2M9QX4TO",
        "https://lesson.example.org/v1/tool#7K2M9QX4TU",
        "https://lesson.example.org/v1/tool#7K2M9-X4TB",
        "https://lesson.example.org/v1/tool#7K2M9QX4TB#7K2M9QX4TB",
        "https://user@lesson.example.org/v1/tool#7K2M9QX4TB",       // userinfo
        "https://user:pw@lesson.example.org/v1/tool#7K2M9QX4TB",
        "https://lesson.example.org/v1/tool?x=1#7K2M9QX4TB",        // query
        "https://lesson.example.org?x=1/v1/tool#7K2M9QX4TB",
        "https://lesson.example.org/v1/tool/#7K2M9QX4TB",           // path
        "https://lesson.example.org/v1/tools#7K2M9QX4TB",
        "https://lesson.example.org/V1/TOOL#7K2M9QX4TB",
        "https://lesson.example.org/#7K2M9QX4TB",
        "https://lesson.example.org#7K2M9QX4TB",
        "https:///v1/tool#7K2M9QX4TB",                              // no host
        "https://lesson..example.org/v1/tool#7K2M9QX4TB",
        "https://lesson_example.org/v1/tool#7K2M9QX4TB",
        "https://lesson.example.org:0/v1/tool#7K2M9QX4TB",          // port
        "https://lesson.example.org:65536/v1/tool#7K2M9QX4TB",
        "https://lesson.example.org:/v1/tool#7K2M9QX4TB",
        "https://lesson.example.org:44a/v1/tool#7K2M9QX4TB",
        "https://[::1/v1/tool#7K2M9QX4TB",
        "https://lesson.example.org/v1/tool#7K2M9 QX4TB",           // inner whitespace
        "https://lesson.example.org/v1/tool#7K2M9QX4TB extra",
        "https://\xE6\x8E\x88\xE6\xA5\xAD.example.org/v1/tool#7K2M9QX4TB",
    };
    for (const char* text : cases) {
        const auto r = parse_lesson_link(text);
        EXPECT_FALSE(r.link.has_value());
        EXPECT_FALSE(r.error.empty());
        EXPECT_FALSE(contains(r.error, "7K2M9"));
        EXPECT_FALSE(contains(r.error, "example"));
    }
    // Different mistakes get different reasons.
    EXPECT_NE(parse_lesson_link("https://a.example/v1/tool").error, parse_lesson_link("https://a.example/x#7K2M9QX4TB").error);
    EXPECT_NE(parse_lesson_link("ftp://a.example/v1/tool#7K2M9QX4TB").error,
              parse_lesson_link("https://u@a.example/v1/tool#7K2M9QX4TB").error);
}

// ---- protocol ------------------------------------------------------------------------

TEST(StudioLessonProtocol, PairReplyAndPollClamp) {
    const auto ok = parse_pair_reply(pair_body("student"));
    ASSERT_TRUE(ok.value.has_value());
    EXPECT_EQ(ok.value->token, std::string(kToken));
    EXPECT_TRUE(ok.value->role == Role::Student);
    EXPECT_EQ(ok.value->class_title, std::string("Action"));
    EXPECT_EQ(ok.value->poll_ms, 2000);
    EXPECT_TRUE(parse_pair_reply(pair_body("presenter")).value->role == Role::Presenter);

    EXPECT_EQ(clamp_poll_ms(1), 1000);
    EXPECT_EQ(clamp_poll_ms(-5), 1000);
    EXPECT_EQ(clamp_poll_ms(1000), 1000);
    EXPECT_EQ(clamp_poll_ms(2500), 2500);
    EXPECT_EQ(clamp_poll_ms(10000), 10000);
    EXPECT_EQ(clamp_poll_ms(600000), 10000);

    const char* const bad[] = {
        "",
        "not json",
        "{\"token\":",
        "[]",
        R"({"token":"0123456789abcdef0123456789abcde","role":"student","class":{"title":"x"},"poll_ms":2000})",    // 31
        R"({"token":"0123456789ABCDEF0123456789ABCDEF","role":"student","class":{"title":"x"},"poll_ms":2000})",   // upper
        R"({"token":12,"role":"student","class":{"title":"x"},"poll_ms":2000})",
        R"({"token":"0123456789abcdef0123456789abcdef","role":"teacher","class":{"title":"x"},"poll_ms":2000})",
        R"({"token":"0123456789abcdef0123456789abcdef","role":"student","poll_ms":2000})",
        R"({"token":"0123456789abcdef0123456789abcdef","role":"student","class":{"title":"x"}})",
        R"({"token":"0123456789abcdef0123456789abcdef","role":"student","class":{"title":"x"},"poll_ms":"2000"})",
    };
    for (const char* body : bad) {
        const auto r = parse_pair_reply(body);
        EXPECT_FALSE(r.value.has_value());
        EXPECT_FALSE(r.error.empty());
        EXPECT_FALSE(contains(r.error, "0123456789abcdef"));   // never the token
    }
}

TEST(StudioLessonProtocol, StateForBothRoles) {
    const auto s = parse_state(kStudentState);
    ASSERT_TRUE(s.value.has_value());
    EXPECT_TRUE(s.value->role == Role::Student);
    EXPECT_EQ(s.value->features.at("hit_stop"), std::string("feel.hit_stop"));
    EXPECT_FALSE(s.value->demo.has_value());
    ASSERT_TRUE(s.value->assignment.has_value());
    const Assignment& a = *s.value->assignment;
    EXPECT_EQ(a.id, std::string("restore-feel"));
    EXPECT_TRUE(a.active);
    ASSERT_EQ(a.steps.size(), 3u);
    EXPECT_FALSE(a.steps[0].check.has_value());
    ASSERT_TRUE(a.steps[1].check.has_value());
    EXPECT_EQ(a.steps[1].check->variable, std::string("feel.hit_stop_last_seconds"));
    EXPECT_TRUE(a.steps[1].check->op == CheckOp::Gt);
    EXPECT_TRUE(a.steps[1].check->value.is_number());
    EXPECT_TRUE(a.steps[2].done);
    EXPECT_TRUE(a.steps[2].check->value.is_bool());

    // The contract's example: no assignment, an empty demo for a student is tolerated.
    const auto idle = parse_state(R"({"role":"student","class":{"title":"t"},"features":{},"assignment":null,"demo":{"seq":0,"states":{}}})");
    ASSERT_TRUE(idle.value.has_value());
    EXPECT_FALSE(idle.value->assignment.has_value());
    EXPECT_TRUE(idle.value->features.empty());

    const auto p = parse_state(presenter_state(3, R"({"hit_stop":true,"knockback":false})"));
    ASSERT_TRUE(p.value.has_value());
    EXPECT_TRUE(p.value->role == Role::Presenter);
    ASSERT_TRUE(p.value->demo.has_value());
    EXPECT_EQ(p.value->demo->seq, 3u);
    EXPECT_TRUE(p.value->demo->states.at("hit_stop"));
    EXPECT_FALSE(p.value->demo->states.at("knockback"));
}

TEST(StudioLessonProtocol, MalformedStateIsAnExplicitError) {
    const char* const bad[] = {
        "",
        "{",
        "null",
        "\"student\"",
        R"({"class":{"title":"t"},"features":{},"assignment":null})",                                    // no role
        R"({"role":"student","features":{},"assignment":null})",                                          // no class
        R"({"role":"student","class":{"title":"t"},"assignment":null})",                                  // no features
        R"({"role":"student","class":{"title":"t"},"features":[],"assignment":null})",
        R"({"role":"student","class":{"title":"t"},"features":{"a":1},"assignment":null})",
        R"({"role":"student","class":{"title":"t"},"features":{},"assignment":"x"})",
        R"({"role":"student","class":{"title":"t"},"features":{},"assignment":{"id":"a","title":"t","active":true}})",
        R"({"role":"student","class":{"title":"t"},"features":{},"assignment":{"id":"a","title":"t","active":true,"steps":[{"id":"s"}]}})",
        R"({"role":"student","class":{"title":"t"},"features":{},"assignment":{"id":"a","title":"t","active":true,"steps":[{"id":"s","label":"l","done":false,"check":{"variable":"v","op":"between","value":1}}]}})",
        R"({"role":"student","class":{"title":"t"},"features":{},"assignment":{"id":"a","title":"t","active":true,"steps":[{"id":"s","label":"l","done":false,"check":{"variable":"v","op":"gt","value":[1]}}]}})",
        R"({"role":"student","class":{"title":"t"},"features":{},"assignment":{"id":"a","title":"t","active":true,"steps":[{"id":"s","label":"l","done":false,"check":{"op":"gt","value":1}}]}})",
        R"({"role":"presenter","class":{"title":"t"},"features":{},"assignment":null})",                 // presenter without demo
        R"({"role":"presenter","class":{"title":"t"},"features":{},"assignment":null,"demo":{"seq":-1,"states":{}}})",
        R"({"role":"presenter","class":{"title":"t"},"features":{},"assignment":null,"demo":{"seq":1.5,"states":{}}})",
        R"({"role":"presenter","class":{"title":"t"},"features":{},"assignment":null,"demo":{"seq":1,"states":{"a":"on"}}})",
        R"({"role":"presenter","class":{"title":"t"},"features":{},"assignment":null,"demo":{"seq":1}})",
    };
    for (const char* body : bad) {
        const auto r = parse_state(body);
        EXPECT_FALSE(r.value.has_value());
        EXPECT_FALSE(r.error.empty());
    }
    EXPECT_TRUE(parse_progress_ack(R"({"ok":true})").empty());
    EXPECT_FALSE(parse_progress_ack(R"({"ok":false})").empty());
    EXPECT_FALSE(parse_progress_ack("ok").empty());
}

TEST(StudioLessonProtocol, RequestBodiesAndFailureClasses) {
    JsonValue pair;
    ASSERT_TRUE(ergo::common::jsonm::parse(pair_request_body("7K2M9QX4TB"), pair));
    EXPECT_EQ(pair.o->size(), 2u);
    EXPECT_EQ(field(pair, "link")->as_string(), std::string("7K2M9QX4TB"));
    EXPECT_EQ(field(pair, "tool")->as_string(), std::string("ergo-studio"));

    JsonValue progress;
    ASSERT_TRUE(ergo::common::jsonm::parse(progress_request_body("restore-feel", 2), progress));
    EXPECT_EQ(progress.o->size(), 3u);
    EXPECT_EQ(field(progress, "assignment_id")->as_string(), std::string("restore-feel"));
    EXPECT_EQ(field(progress, "step")->as_number(-1), 2.0);
    EXPECT_TRUE(field(progress, "done")->as_bool());
    EXPECT_TRUE(contains(progress_request_body("a", 2), "\"step\":2"));   // an integer on the wire

    platform::HttpClientResponse res;
    res.error = "connection refused";
    EXPECT_TRUE(classify_response(res).kind == FailureKind::Transport);
    EXPECT_EQ(classify_response(res).detail, std::string("connection refused"));
    res.timed_out = true;
    EXPECT_TRUE(classify_response(res).kind == FailureKind::Timeout);

    res              = {};
    res.transport_ok = true;
    res.status       = 200;
    EXPECT_FALSE(classify_response(res).failed());
    res.status = 404;
    res.body   = R"({"error":"link_not_found"})";
    EXPECT_TRUE(classify_response(res).is_http(404, "link_not_found"));
    res.status = 429;
    res.body   = R"({"error":"rate_limited"})";
    EXPECT_TRUE(classify_response(res).is_http(429, "rate_limited"));
    res.status = 502;
    res.body   = "<html>Bad Gateway</html>";
    EXPECT_TRUE(classify_response(res).kind == FailureKind::Http);
    EXPECT_EQ(classify_response(res).status, 502);
    EXPECT_TRUE(classify_response(res).code.empty());
    res.body = R"({"error":"<script>alert(1)</script>"})";   // not a plain code: not shown
    EXPECT_TRUE(classify_response(res).code.empty());
}

// ---- step check ----------------------------------------------------------------------

TEST(StudioLessonStepCheck, EveryOperatorAndKindMismatch) {
    struct Case {
        const char*  op;
        JsonValue    expected;
        const char*  kind;
        JsonValue    actual;
        bool         satisfied;
    };
    const Case cases[] = {
        {"gt", jnum(0), "double", jnum(0.12), true},
        {"gt", jnum(0), "double", jnum(0), false},
        {"gt", jnum(0), "float", jnum(-1), false},
        {"ge", jnum(3), "int32", jnum(3), true},
        {"ge", jnum(3), "int32", jnum(2), false},
        {"lt", jnum(3), "int64", jnum(2), true},
        {"lt", jnum(3), "int64", jnum(3), false},
        {"le", jnum(3), "float", jnum(3), true},
        {"le", jnum(3), "float", jnum(3.5), false},
        {"eq", jnum(3), "int32", jnum(3), true},
        {"eq", jnum(3), "int32", jnum(4), false},
        {"ne", jnum(3), "int32", jnum(4), true},
        {"ne", jnum(3), "int32", jnum(3), false},
        {"eq", jbool(true), "bool", jbool(true), true},
        {"eq", jbool(true), "bool", jbool(false), false},
        {"ne", jbool(true), "bool", jbool(false), true},
        {"ne", jbool(true), "bool", jbool(true), false},
        {"eq", jstr("boss"), "string", jstr("boss"), true},
        {"eq", jstr("boss"), "string", jstr("title"), false},
        {"ne", jstr("boss"), "string", jstr("title"), true},
        {"ne", jstr("boss"), "string", jstr("boss"), false},
        // kind mismatches are "not satisfied", also for ne
        {"gt", jnum(0), "bool", jbool(true), false},
        {"gt", jnum(0), "string", jstr("1"), false},
        {"eq", jnum(1), "bool", jbool(true), false},
        {"ne", jnum(1), "bool", jbool(true), false},
        {"ne", jnum(1), "string", jstr("x"), false},
        {"eq", jbool(true), "int32", jnum(1), false},
        {"ne", jbool(true), "int32", jnum(0), false},
        {"eq", jstr("1"), "int32", jnum(1), false},
        {"ne", jstr("a"), "bool", jbool(true), false},
        {"eq", jnum(1), "color", jnum(1), false},              // numeric JSON, but not a numeric kind
        {"eq", jstr("#fff"), "color", jstr("#fff"), false},
        {"eq", jnum(1), "double", jstr("1"), false},           // the kind says number, the value does not
        // ordering needs numbers
        {"gt", jbool(false), "bool", jbool(true), false},
        {"lt", jstr("a"), "string", jstr("b"), false},
        {"ge", jbool(true), "bool", jbool(true), false},
        {"le", jstr("a"), "string", jstr("a"), false},
    };
    for (const auto& c : cases) {
        const auto op = parse_check_op(c.op);
        ASSERT_TRUE(op.has_value());
        const StepCheck check{"v", *op, c.expected};
        EXPECT_EQ(check_satisfied(check, GameVariable{c.kind, c.actual}), c.satisfied);
        EXPECT_FALSE(check_satisfied(check, std::nullopt));   // a missing variable never satisfies
    }
    EXPECT_FALSE(parse_check_op("GT").has_value());
    EXPECT_FALSE(parse_check_op("").has_value());
    EXPECT_FALSE(parse_check_op("between").has_value());
}

// ---- student -------------------------------------------------------------------------

TEST(StudioLessonStudent, PairsWithTheCodeAndPollsWithTheBearerToken) {
    Linked t("student");
    ASSERT_TRUE(t.session.linked());
    ASSERT_EQ(t.http->seen.size(), 1u);
    EXPECT_EQ(t.http->header(0, "Content-Type"), std::string("application/json"));
    EXPECT_TRUE(t.http->header(0, "Authorization").empty());
    JsonValue body;
    ASSERT_TRUE(ergo::common::jsonm::parse(t.http->seen[0].body, body));
    EXPECT_EQ(field(body, "link")->as_string(), std::string("7K2M9QX4TB"));
    EXPECT_EQ(field(body, "tool")->as_string(), std::string("ergo-studio"));
    EXPECT_EQ(t.session.poll_ms(), 2000);

    t.http->routes[kStateUrl] = {200, kStudentState};
    t.session.poll();
    ASSERT_EQ(t.http->seen.size(), 2u);
    EXPECT_EQ(t.http->seen[1].method, std::string("GET"));
    EXPECT_EQ(t.http->header(1, "Authorization"), std::string("Bearer ") + kToken);

    const SessionView v = t.session.view();
    EXPECT_TRUE(v.linked);
    EXPECT_TRUE(v.role == Role::Student);
    EXPECT_EQ(v.host, std::string("127.0.0.1"));
    EXPECT_EQ(v.class_title, std::string("Action"));
    ASSERT_TRUE(v.assignment.has_value());
    ASSERT_EQ(v.assignment->steps.size(), 3u);
    EXPECT_FALSE(v.assignment->steps[0].automatic);
    EXPECT_TRUE(v.assignment->steps[1].automatic);
    EXPECT_FALSE(v.assignment->steps[1].done);
    EXPECT_TRUE(v.assignment->steps[2].done);
}

TEST(StudioLessonStudent, ASatisfiedCheckIsReportedExactlyOnce) {
    Linked t("student");
    t.http->routes[kStateUrl]    = {200, kStudentState};
    t.http->routes[kProgressUrl] = {200, R"({"ok":true})"};
    // Step 2's check holds too, but the server already has it as done.
    t.vars->vars["feel.camera_shake"]           = var("bool", jbool(true));
    t.vars->vars["feel.hit_stop_last_seconds"]  = var("double", jnum(0));

    t.session.poll();
    EXPECT_EQ(count(*t.http, kProgressUrl), 0u);   // not satisfied yet; done steps and steps without a check untouched

    t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0.12));
    t.session.poll();
    ASSERT_EQ(count(*t.http, kProgressUrl), 1u);
    const auto& post = t.http->seen.back();
    EXPECT_EQ(t.http->header(t.http->seen.size() - 1, "Authorization"), std::string("Bearer ") + kToken);
    EXPECT_EQ(t.http->header(t.http->seen.size() - 1, "Content-Type"), std::string("application/json"));
    JsonValue body;
    ASSERT_TRUE(ergo::common::jsonm::parse(post.body, body));
    EXPECT_EQ(body.o->size(), 3u);
    EXPECT_EQ(field(body, "assignment_id")->as_string(), std::string("restore-feel"));
    EXPECT_EQ(field(body, "step")->as_number(-1), 1.0);
    EXPECT_TRUE(field(body, "done")->is_bool());
    EXPECT_TRUE(field(body, "done")->b);
    EXPECT_TRUE(t.session.view().assignment->steps[1].done);
    EXPECT_FALSE(t.session.view().failure.failed());

    // The server still answers done:false (it lags), and the value drops back: no second POST, no un-tick.
    t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0));
    t.session.poll();
    t.session.poll();
    EXPECT_EQ(count(*t.http, kProgressUrl), 1u);
    EXPECT_TRUE(t.session.view().assignment->steps[1].done);
    for (const auto& r : t.http->seen) EXPECT_FALSE(contains(r.body, "false"));   // done:false is never sent
}

TEST(StudioLessonStudent, AMissingOrMismatchedVariableIsNotAnError) {
    Linked t("student");
    t.http->routes[kStateUrl]    = {200, kStudentState};
    t.http->routes[kProgressUrl] = {200, R"({"ok":true})"};
    t.session.poll();   // no game connected
    t.vars->vars["feel.hit_stop_last_seconds"] = var("bool", jbool(true));   // wrong kind
    t.session.poll();
    EXPECT_EQ(count(*t.http, kProgressUrl), 0u);
    EXPECT_FALSE(t.session.view().failure.failed());
    EXPECT_TRUE(t.session.linked());
}

TEST(StudioLessonStudent, ATransportFailureIsRetriedOnTheNextPoll) {
    Linked t("student");
    t.http->routes[kStateUrl] = {200, kStudentState};
    t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0.5));

    t.session.poll();   // no progress route: FakeHttp fails at the transport level
    EXPECT_EQ(count(*t.http, kProgressUrl), 1u);
    SessionView v = t.session.view();
    EXPECT_TRUE(v.failed_call == LessonCall::Progress);
    EXPECT_TRUE(v.failure.kind == FailureKind::Transport);
    EXPECT_FALSE(v.assignment->steps[1].done);
    EXPECT_TRUE(v.linked);

    // The value went back before the retry: the step was achieved, so it is still reported (latched).
    t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0));
    t.http->routes[kProgressUrl]               = {200, R"({"ok":true})"};
    t.session.poll();
    EXPECT_EQ(count(*t.http, kProgressUrl), 2u);
    v = t.session.view();
    EXPECT_FALSE(v.failure.failed());
    EXPECT_TRUE(v.assignment->steps[1].done);

    t.session.poll();
    EXPECT_EQ(count(*t.http, kProgressUrl), 2u);
}

TEST(StudioLessonStudent, AServerErrorOnProgressIsShownAndRetried) {
    Linked t("student");
    t.http->routes[kStateUrl]    = {200, kStudentState};
    t.http->routes[kProgressUrl] = {500, "oops"};
    t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0.5));
    t.session.poll();
    EXPECT_TRUE(t.session.view().failure.kind == FailureKind::Http);
    EXPECT_EQ(t.session.view().failure.status, 500);
    t.http->routes[kProgressUrl] = {200, "not json"};
    t.session.poll();
    EXPECT_TRUE(t.session.view().failure.kind == FailureKind::Malformed);
    EXPECT_FALSE(t.session.view().assignment->steps[1].done);
    EXPECT_EQ(count(*t.http, kProgressUrl), 2u);
}

TEST(StudioLessonStudent, NotAssignedStopsReportingForThatAssignment) {
    Linked t("student");
    t.http->routes[kStateUrl]    = {200, kStudentState};
    t.http->routes[kProgressUrl] = {409, R"({"error":"not_assigned"})"};
    t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0.5));
    t.session.poll();
    t.session.poll();
    t.session.poll();
    EXPECT_EQ(count(*t.http, kProgressUrl), 1u);
    EXPECT_TRUE(t.session.linked());
    EXPECT_FALSE(t.session.view().failure.failed());   // the assignment ended: not an error
    EXPECT_FALSE(t.session.view().assignment->steps[1].done);
}

TEST(StudioLessonStudent, AnInactiveAssignmentIsNotReported) {
    Linked t("student");
    std::string inactive = kStudentState;
    inactive.replace(inactive.find("\"active\":true"), 13, "\"active\":false");
    t.http->routes[kStateUrl]    = {200, inactive};
    t.http->routes[kProgressUrl] = {200, R"({"ok":true})"};
    t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0.5));
    t.session.poll();
    EXPECT_EQ(count(*t.http, kProgressUrl), 0u);
}

TEST(StudioLessonStudent, ToolUnlinkedDropsTheTokenAndStopsPolling) {
    {
        Linked t("student");   // 401 on state
        t.http->routes[kStateUrl] = {401, R"({"error":"tool_unlinked"})"};
        t.session.poll();
        EXPECT_FALSE(t.session.linked());
        const SessionView v = t.session.view();
        EXPECT_FALSE(v.linked);
        EXPECT_TRUE(v.end == LinkEnd::Unlinked);
        EXPECT_TRUE(v.role == Role::None);
        const std::size_t sent = t.http->seen.size();
        t.session.poll();
        t.session.poll();
        EXPECT_EQ(t.http->seen.size(), sent);   // nothing more is sent
    }
    {
        Linked t("student");   // 401 on progress
        t.http->routes[kStateUrl]    = {200, kStudentState};
        t.http->routes[kProgressUrl] = {401, R"({"error":"tool_unlinked"})"};
        t.vars->vars["feel.hit_stop_last_seconds"] = var("double", jnum(0.5));
        t.session.poll();
        EXPECT_FALSE(t.session.linked());
        EXPECT_TRUE(t.session.view().end == LinkEnd::Unlinked);
        const std::size_t sent = t.http->seen.size();
        t.session.poll();
        EXPECT_EQ(t.http->seen.size(), sent);
    }
    {
        Linked t("student");   // another 401 is an error, not the end of the link
        t.http->routes[kStateUrl] = {401, R"({"error":"something_else"})"};
        t.session.poll();
        EXPECT_TRUE(t.session.linked());
        EXPECT_TRUE(t.session.view().failed_call == LessonCall::State);
        EXPECT_EQ(t.session.view().failure.status, 401);
    }
}

TEST(StudioLessonSession, PairFailuresAreClassifiedAndLeaveTheSessionUnlinked) {
    auto http = std::make_shared<FakeHttp>();
    auto vars = std::make_shared<FakeVars>();
    LessonSession session(LessonClient(http), vars);
    const auto link = *parse_lesson_link(kLink).link;

    session.link(link);   // no route: transport
    EXPECT_FALSE(session.linked());
    EXPECT_TRUE(session.view().failed_call == LessonCall::Pair);
    EXPECT_TRUE(session.view().failure.kind == FailureKind::Transport);

    http->routes[kPairUrl] = {404, R"({"error":"link_not_found"})"};
    session.link(link);
    EXPECT_TRUE(session.view().failure.is_http(404, "link_not_found"));
    http->routes[kPairUrl] = {429, R"({"error":"rate_limited"})"};
    session.link(link);
    EXPECT_TRUE(session.view().failure.is_http(429, "rate_limited"));
    http->routes[kPairUrl] = {200, "{\"token\":\"short\"}"};
    session.link(link);
    EXPECT_TRUE(session.view().failure.kind == FailureKind::Malformed);
    EXPECT_FALSE(session.linked());
    session.poll();   // unlinked: nothing is sent
    EXPECT_EQ(count(*http, kStateUrl), 0u);

    http->routes[kPairUrl] = {200, pair_body("student")};
    session.link(link);
    EXPECT_TRUE(session.linked());
    EXPECT_FALSE(session.view().failure.failed());
    session.disconnect();
    EXPECT_FALSE(session.linked());
    EXPECT_TRUE(session.view().end == LinkEnd::Disconnected);
}

TEST(StudioLessonSession, AFailedStatePollKeepsTheLinkAndRecovers) {
    Linked t("student");
    t.session.poll();   // no state route: transport failure
    EXPECT_TRUE(t.session.linked());
    EXPECT_TRUE(t.session.view().failed_call == LessonCall::State);
    t.http->routes[kStateUrl] = {200, "{\"role\":"};
    t.session.poll();
    EXPECT_TRUE(t.session.view().failure.kind == FailureKind::Malformed);
    EXPECT_TRUE(t.session.linked());
    t.http->routes[kStateUrl] = {200, kStudentState};
    t.session.poll();
    EXPECT_FALSE(t.session.view().failure.failed());
    EXPECT_TRUE(t.session.view().assignment.has_value());
}

// ---- presenter -----------------------------------------------------------------------

TEST(StudioLessonPresenter, ASeqChangeAppliesTheDemoStates) {
    Linked t("presenter");
    t.vars->vars["feel.hit_stop"]  = var("bool", jbool(false));
    t.vars->vars["feel.knockback"] = var("bool", jbool(true));

    t.http->routes[kStateUrl] = {200, presenter_state(0, "{}")};
    t.session.poll();
    EXPECT_TRUE(t.vars->sets.empty());
    ASSERT_EQ(t.session.view().demo.size(), 2u);
    EXPECT_TRUE(t.session.view().demo[0].status == DemoStatus::NotCued);

    t.http->routes[kStateUrl] = {200, presenter_state(1, R"({"hit_stop":true,"knockback":false})")};
    t.session.poll();
    ASSERT_EQ(t.vars->sets.size(), 2u);
    EXPECT_EQ(t.vars->sets[0].first, std::string("feel.hit_stop"));
    EXPECT_TRUE(t.vars->sets[0].second.is_bool());
    EXPECT_TRUE(t.vars->sets[0].second.b);
    EXPECT_EQ(t.vars->sets[1].first, std::string("feel.knockback"));
    EXPECT_FALSE(t.vars->sets[1].second.b);
    for (const auto& e : t.session.view().demo) EXPECT_TRUE(e.status == DemoStatus::Applied);

    t.session.poll();   // same seq: nothing is repeated
    EXPECT_EQ(t.vars->sets.size(), 2u);

    t.http->routes[kStateUrl] = {200, presenter_state(2, R"({"hit_stop":false,"knockback":false})")};
    t.session.poll();   // a new cue applies every cued feature again
    ASSERT_EQ(t.vars->sets.size(), 4u);
    EXPECT_FALSE(t.vars->sets[2].second.b);
    EXPECT_EQ(count(*t.http, kProgressUrl), 0u);   // a presenter never reports progress
}

TEST(StudioLessonPresenter, AnUnboundVariableStaysPendingUntilTheGameBindsIt) {
    Linked t("presenter");
    t.vars->vars["feel.knockback"] = var("bool", jbool(false));
    t.http->routes[kStateUrl] = {200, presenter_state(5, R"({"hit_stop":true,"knockback":true})")};

    t.session.poll();   // joined mid-class: seq 5 is new to this session
    ASSERT_EQ(t.vars->sets.size(), 1u);
    EXPECT_EQ(t.vars->sets[0].first, std::string("feel.knockback"));
    auto demo = t.session.view().demo;
    ASSERT_EQ(demo.size(), 2u);
    EXPECT_EQ(demo[0].feature, std::string("hit_stop"));
    EXPECT_TRUE(demo[0].status == DemoStatus::WaitingForGame);
    EXPECT_TRUE(demo[0].on.value_or(false));
    EXPECT_TRUE(demo[1].status == DemoStatus::Applied);

    t.session.poll();   // still no game for it
    EXPECT_EQ(t.vars->sets.size(), 1u);

    t.vars->vars["feel.hit_stop"] = var("int32", jnum(0));   // bound, but not a bool: not set, and said so
    t.session.poll();
    EXPECT_EQ(t.vars->sets.size(), 1u);
    EXPECT_TRUE(t.session.view().demo[0].status == DemoStatus::NotBool);

    t.vars->vars["feel.hit_stop"] = var("bool", jbool(false));
    t.session.poll();
    ASSERT_EQ(t.vars->sets.size(), 2u);
    EXPECT_EQ(t.vars->sets[1].first, std::string("feel.hit_stop"));
    EXPECT_TRUE(t.vars->sets[1].second.b);
    EXPECT_TRUE(t.session.view().demo[0].status == DemoStatus::Applied);

    t.session.poll();
    EXPECT_EQ(t.vars->sets.size(), 2u);
}

TEST(StudioLessonPresenter, APendingSwitchIsSupersededByTheNextCue) {
    Linked t("presenter");
    t.http->routes[kStateUrl] = {200, presenter_state(1, R"({"hit_stop":true})")};
    t.session.poll();
    EXPECT_TRUE(t.session.view().demo[0].status == DemoStatus::WaitingForGame);
    t.http->routes[kStateUrl] = {200, presenter_state(2, R"({"hit_stop":false})")};
    t.session.poll();
    t.vars->vars["feel.hit_stop"] = var("bool", jbool(true));
    t.session.poll();
    ASSERT_EQ(t.vars->sets.size(), 1u);
    EXPECT_FALSE(t.vars->sets[0].second.b);   // only the latest cue reaches the game
}

TEST(StudioLessonPresenter, AFeatureWithoutAVariableIsReported) {
    Linked t("presenter");
    t.vars->vars["feel.hit_stop"] = var("bool", jbool(false));
    t.http->routes[kStateUrl] = {200, presenter_state(1, R"({"hit_stop":true,"slow_motion":true})")};
    t.session.poll();
    EXPECT_EQ(t.vars->sets.size(), 1u);
    const auto demo = t.session.view().demo;
    ASSERT_EQ(demo.size(), 3u);   // hit_stop, knockback (not cued), slow_motion (no variable)
    EXPECT_TRUE(demo[1].status == DemoStatus::NotCued);
    EXPECT_EQ(demo[2].feature, std::string("slow_motion"));
    EXPECT_TRUE(demo[2].status == DemoStatus::NoVariable);
    EXPECT_TRUE(demo[2].variable.empty());
}

// ---- bridge to the variable plugin ------------------------------------------------------

TEST(StudioLessonVariables, TheBridgeReadsAndSetsThroughTheVariablePlugin) {
    auto bridge = std::make_shared<app::GameVariableBridge>();
    EXPECT_FALSE(bridge->find("feel.hit_stop").has_value());   // no variable plugin yet
    EXPECT_FALSE(bridge->request_set("feel.hit_stop", jbool(true)));

    auto plugin = app::make_bridged_variable_plugin(bridge, [](const std::string&, const std::string&) {});
    EXPECT_EQ(plugin->info().id, std::string("variable"));
    ASSERT_TRUE(plugin->socket() != nullptr);
    ASSERT_TRUE(plugin->http() != nullptr);
    ASSERT_TRUE(plugin->health() != nullptr);

    auto engine = std::make_shared<FakeConn>(1);
    auto h      = plugin->socket()->on_open(net::HttpRequest{}, engine);
    h.on_text(R"({"op":"hello","role":"engine","app":"game"})");
    EXPECT_FALSE(bridge->find("feel.hit_stop").has_value());
    h.on_text(R"({"op":"bind","name":"feel.hit_stop","kind":"bool","value":false})");
    h.on_text(R"({"op":"bind","name":"feel.hit_stop_last_seconds","kind":"double","value":0})");
    h.on_text(R"({"op":"value","name":"feel.hit_stop_last_seconds","value":0.25})");

    const auto seconds = bridge->find("feel.hit_stop_last_seconds");
    ASSERT_TRUE(seconds.has_value());
    EXPECT_EQ(seconds->kind, std::string("double"));
    EXPECT_EQ(seconds->value.as_number(), 0.25);
    const auto toggle = bridge->find("feel.hit_stop");
    ASSERT_TRUE(toggle.has_value());
    EXPECT_EQ(toggle->kind, std::string("bool"));
    EXPECT_FALSE(bridge->find("feel.unknown").has_value());

    EXPECT_FALSE(bridge->request_set("feel.unknown", jbool(true)));
    ASSERT_TRUE(bridge->request_set("feel.hit_stop", jbool(true)));
    const auto frame = engine->last("set");
    EXPECT_EQ(field(frame, "name")->as_string(), std::string("feel.hit_stop"));
    EXPECT_TRUE(field(frame, "value")->as_bool());

    h.on_close();   // the game went away
    EXPECT_FALSE(bridge->find("feel.hit_stop").has_value());

    plugin.reset();   // the variable plugin is destroyed before the lesson plugin
    EXPECT_FALSE(bridge->find("feel.hit_stop").has_value());
    EXPECT_FALSE(bridge->request_set("feel.hit_stop", jbool(true)));
}

// ---- plugin ---------------------------------------------------------------------------

namespace {

bool wait_until(const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

std::vector<std::string>* g_log = nullptr;
std::mutex                g_log_mtx;
void                      collect_log(const std::string& level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_log_mtx);
    if (g_log) g_log->push_back(level + " " + msg);
}

} // namespace

TEST(StudioLessonPlugin, HealthAndRejectedLinksBeforeAnyConnection) {
    auto http = std::make_shared<FakeHttp>();
    LessonPlugin plugin(http, std::make_shared<FakeVars>(), [](const std::string&, const std::string&) {});
    EXPECT_EQ(plugin.info().id, std::string("lesson"));
    EXPECT_TRUE(host::is_valid_plugin_id(plugin.info().id));
    EXPECT_TRUE(plugin.http() == nullptr);     // no routes on the Studio's own server
    EXPECT_TRUE(plugin.socket() == nullptr);

    JsonValue h = plugin.snapshot();
    EXPECT_EQ(h.o->size(), 3u);
    EXPECT_TRUE(field(h, "ok")->as_bool());
    EXPECT_FALSE(field(h, "linked")->as_bool(true));
    EXPECT_EQ(field(h, "role")->as_string(), std::string("none"));

    const uint64_t before = plugin.revision();
    plugin.connect("https://lesson.example.org/v1/tool");
    EXPECT_GT(plugin.revision(), before);
    EXPECT_FALSE(plugin.shown().link_error.empty());
    EXPECT_FALSE(plugin.shown().connecting);
    EXPECT_FALSE(plugin.shown().session.linked);
}   // the destructor joins the idle worker

TEST(StudioLessonPlugin, LinksOnTheWorkerAndKeepsSecretsOutOfHealthAndLogs) {
    std::vector<std::string> log;
    {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        g_log = &log;
    }
    auto http = std::make_shared<FakeHttp>();
    http->routes[kPairUrl]  = {200, pair_body("student")};
    http->routes[kStateUrl] = {200, kStudentState};
    std::string health;
    {
        LessonPlugin plugin(http, std::make_shared<FakeVars>(), &collect_log);
        plugin.connect(kLink);
        ASSERT_TRUE(wait_until([&] { return plugin.shown().session.linked && plugin.shown().session.assignment.has_value(); }));
        EXPECT_FALSE(plugin.shown().connecting);
        EXPECT_EQ(plugin.shown().session.host, std::string("127.0.0.1"));
        health = to_json(plugin.snapshot());
        EXPECT_TRUE(field(plugin.snapshot(), "linked")->as_bool());
        EXPECT_EQ(field(plugin.snapshot(), "role")->as_string(), std::string("student"));
        EXPECT_EQ(plugin.snapshot().o->size(), 3u);

        plugin.disconnect();
        ASSERT_TRUE(wait_until([&] { return !plugin.shown().session.linked; }));
        EXPECT_TRUE(plugin.shown().session.end == LinkEnd::Disconnected);
        EXPECT_EQ(field(plugin.snapshot(), "role")->as_string(), std::string("none"));

        http->routes[kStateUrl] = {401, R"({"error":"tool_unlinked"})"};   // read by the worker only after connect()
        plugin.connect(kLink);
        ASSERT_TRUE(wait_until([&] { return plugin.shown().session.end == LinkEnd::Unlinked; }));
        EXPECT_FALSE(plugin.shown().session.linked);
    }   // joined: the fake and the log are no longer written
    {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        g_log = nullptr;
    }
    EXPECT_EQ(count(*http, kPairUrl), 2u);
    EXPECT_FALSE(contains(health, kToken));
    EXPECT_FALSE(contains(health, "7K2M9QX4TB"));
    EXPECT_FALSE(log.empty());
    for (const auto& line : log) {
        EXPECT_FALSE(contains(line, kToken));
        EXPECT_FALSE(contains(line, "7K2M9QX4TB"));
    }
}
