#pragma once
/// The link to one odeum-lesson class as a state machine driven by explicit
/// calls: link() pairs, poll() runs one cycle (fetch the state, then report
/// satisfied steps as a student or switch demo features as a presenter).
/// No thread and no clock, so it is deterministic under test. Not
/// thread-safe: LessonPlugin calls it from its worker thread only.
///
/// The token lives in this object only (memory); view() never contains it.
#include "demo_apply.h"
#include "lesson_client.h"

#include <memory>
#include <set>

namespace ergo::studio::plugins::lesson {

/// Why there is no link (shown by the view).
enum class LinkEnd {
    None,             ///< never linked, or linked now
    Disconnected,     ///< the user pressed 切る
    Unlinked,         ///< the server answered 401 tool_unlinked: the token is gone for good
};

/// Which call the shown failure belongs to.
enum class LessonCall { None, Pair, State, Progress };

struct StepRow {
    std::string label;
    bool        done      = false;   ///< done on the server, or reported by this session
    bool        automatic = false;   ///< has a check: ticked from the game's variables
};

struct AssignmentRows {
    std::string          title;
    bool                 active = false;
    std::vector<StepRow> steps;
};

/// Everything the view may show. No token, no link code.
struct SessionView {
    bool                          linked = false;
    Role                          role   = Role::None;
    std::string                   host;
    std::string                   class_title;
    LinkEnd                       end = LinkEnd::None;
    LessonCall                    failed_call = LessonCall::None;
    CallFailure                   failure;      ///< of failed_call; cleared by the next successful cycle
    std::optional<AssignmentRows> assignment;   ///< student
    std::vector<DemoEntry>        demo;         ///< presenter
};
bool operator==(const SessionView& a, const SessionView& b);
inline bool operator!=(const SessionView& a, const SessionView& b) { return !(a == b); }

class LessonSession {
public:
    LessonSession(LessonClient client, std::shared_ptr<IGameVariables> variables);

    /// POST pair. Replaces any current link. On failure the session stays unlinked with the failure in view().
    void link(const LessonLink& link);
    /// Forget the token (the user pressed 切る). Nothing is sent.
    void disconnect();
    /// One cycle; a no-op while unlinked.
    void poll();

    bool        linked() const { return !token_.empty(); }
    int         poll_ms() const { return poll_ms_; }
    SessionView view() const;

private:
    void drop(LinkEnd why);
    /// True when `f` ended the link (401 tool_unlinked).
    bool ended_by(const CallFailure& f);
    void fail(LessonCall call, CallFailure f);
    void report_progress();

    LessonClient                    client_;
    std::shared_ptr<IGameVariables> variables_;

    std::string origin_;
    std::string host_;
    std::string token_;   ///< empty = unlinked
    int         poll_ms_ = kMinPollMs;

    Role                      role_ = Role::None;
    std::string               class_title_;
    std::optional<Assignment> assignment_;
    LinkEnd                   end_ = LinkEnd::None;
    LessonCall                failed_call_ = LessonCall::None;
    CallFailure               failure_;

    // Student: progress of the assignment `tracked_id_`. Latched: indices only ever get added.
    std::string           tracked_id_;
    std::set<std::size_t> satisfied_;   ///< the check held at some poll
    std::set<std::size_t> reported_;    ///< the server accepted done=true from this session
    bool                  assignment_ended_ = false;   ///< 409 not_assigned: stop reporting for it

    DemoApplier demo_;   ///< presenter
};

} // namespace ergo::studio::plugins::lesson
