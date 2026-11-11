#pragma once
/// What the 授業 tab shows (LessonShown) as JSON, for GET /lesson/api/state.
/// Same content as the tab: never the token, the link code or the pasted text.
#include "lesson_plugin.h"

namespace ergo::studio::plugins::lesson {

/// {connecting, link_error, linked, role, host, class, end, failure, assignment, demo}
///   end:        "none" | "disconnected" | "unlinked"
///   failure:    null | {call: "pair"|"state"|"progress", kind: "transport"|"timeout"|"http"|"malformed",
///                       status, code, detail}
///   assignment: null | {title, active, steps: [{label, done, automatic}]}   (student)
///   demo:       [{feature, variable, on: bool|null, status}]                (presenter; [] otherwise)
///   demo status: "not_cued" | "applied" | "requested" | "not_following" | "waiting_for_game" | "not_bool" | "no_variable"
JsonValue lesson_shown_json(const LessonShown& shown);

const char* demo_status_id(DemoStatus s);

} // namespace ergo::studio::plugins::lesson
