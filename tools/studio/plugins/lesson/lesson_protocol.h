#pragma once
/// Wire contract with odeum-lesson (spec/feature/tool/lesson.md): JSON bodies
/// of pair / state / progress as structs, and the classification of a failed
/// call. Pure: no I/O. Malformed input becomes an error string, never a throw.
#include "step_check.h"

#include "ergo/studio/platform/http_client.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::lesson {

enum class Role { None, Student, Presenter };
/// "none" / "student" / "presenter"
const char* role_name(Role r);

/// Upper bounds of an odeum-lesson reply (pair / state / progress / an error
/// body), checked before and while it is parsed. A lesson state is a class title,
/// a few dozen steps and features: kilobytes. The reply is polled every 1-10 s,
/// so the HTTP client's 64 MiB cap is far too much to accept per poll.
/// (spec/setup/hardening.md "入力の上限")
constexpr std::size_t kMaxReplyBytes    = 1024u * 1024u;   ///< body (~1000x a real state)
constexpr std::size_t kMaxReplyValues   = 16384;           ///< JSON values of any kind
constexpr std::size_t kMaxReplyDepth    = 16;              ///< nesting (a state uses 5)
constexpr std::size_t kMaxSteps         = 256;             ///< assignment.steps
constexpr std::size_t kMaxFeatures      = 256;             ///< features / demo.states entries, each
constexpr std::size_t kMaxText          = 4096;            ///< bytes of one string (title, label, id, name)
constexpr std::size_t kMaxReplyTextSum  = 256u * 1024u;    ///< bytes of all kept strings together

constexpr int kMinPollMs = 1000;
constexpr int kMaxPollMs = 10000;
/// The server's poll_ms clamped to kMinPollMs..kMaxPollMs.
int clamp_poll_ms(double poll_ms);

struct PairReply {
    std::string token;   ///< 32 lowercase hex. A secret: memory only.
    Role        role = Role::None;
    std::string class_title;
    int         poll_ms = kMinPollMs;   ///< already clamped
};

struct Step {
    std::string              id;
    std::string              label;
    bool                     done = false;
    std::optional<StepCheck> check;
};

struct Assignment {
    std::string       id;
    std::string       title;
    bool              active = false;
    std::vector<Step> steps;
};

struct DemoState {
    uint64_t                    seq = 0;
    std::map<std::string, bool> states;   ///< feature -> on, for every feature the timeline has set
};

struct LessonState {
    Role                               role = Role::None;
    std::string                        class_title;
    std::map<std::string, std::string> features;     ///< feature -> name of a bool variable in the game
    std::optional<Assignment>          assignment;   ///< null on the wire when there is none
    std::optional<DemoState>           demo;         ///< always set for a presenter
};

template <class T>
struct Parsed {
    std::optional<T> value;
    std::string      error;   ///< which part of the body was wrong (no body content)
};

Parsed<PairReply>   parse_pair_reply(const std::string& body);
Parsed<LessonState> parse_state(const std::string& body);
/// Empty when the body is {"ok":true}; otherwise what was wrong.
std::string         parse_progress_ack(const std::string& body);

std::string pair_request_body(const std::string& code);
/// `step` is the 0-based index into assignment.steps. Progress is latched: only done=true is ever sent.
std::string progress_request_body(const std::string& assignment_id, std::size_t step);

enum class FailureKind {
    None,
    Transport,   ///< no HTTP response at all (DNS, refused, TLS, unsupported platform)
    Timeout,     ///< no response in time
    Http,        ///< a response with a non-2xx status
    Malformed,   ///< 2xx, but the body does not follow the contract
};

struct CallFailure {
    FailureKind kind   = FailureKind::None;
    int         status = 0;    ///< Http only
    std::string code;          ///< Http only: the server's {"error":"<code>"}, when it is a plain code
    std::string detail;        ///< Transport: the platform's message. Malformed: what was wrong.

    bool failed() const { return kind != FailureKind::None; }
    bool is_http(int http_status, const char* error_code) const {
        return kind == FailureKind::Http && status == http_status && code == error_code;
    }
};
bool operator==(const CallFailure& a, const CallFailure& b);
inline bool operator!=(const CallFailure& a, const CallFailure& b) { return !(a == b); }

/// None for a 2xx response; the body of a 2xx still has to be parsed by the caller.
CallFailure classify_response(const platform::HttpClientResponse& res);
CallFailure malformed(std::string what);

} // namespace ergo::studio::plugins::lesson
