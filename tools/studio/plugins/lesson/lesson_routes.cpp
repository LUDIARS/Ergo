#include "lesson_routes.h"

#include "lesson_shown_json.h"

#include "../common/mutation_guard.h"

namespace ergo::studio::plugins::lesson {
namespace {

net::HttpResponse error_response(int status, const char* code, const std::string& message) {
    return json_response(status, jobj({{"error", jstr(code)}, {"message", jstr(message)}}));
}

net::HttpResponse connect(LessonPlugin& plugin, const net::HttpRequest& req, const host::LogFn& log) {
    JsonValue body;
    if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) {
        return error_response(400, "invalid_request", "本文は JSON のオブジェクトにしてください");
    }
    const JsonValue* link = field(body, "link");
    if (!link || !link->is_string()) return error_response(400, "invalid_request", "link (文字列) がありません");
    // Like the tab, whose button is off while linked or connecting: disconnect first.
    const LessonShown now = plugin.shown();
    if (now.connecting || now.session.linked) {
        return error_response(409, "already_linked", "すでに授業につながっています。先に /lesson/api/disconnect で切ってください");
    }
    if (const auto rejected = plugin.connect(link->s)) return error_response(400, "invalid_link", *rejected);
    if (log) log("info", "[lesson] connecting (requested through /lesson/api/connect)");   // never the link itself
    return json_response(202, jobj({{"ok", jbool(true)}, {"connecting", jbool(true)}}));
}

} // namespace

net::HttpResponse handle_lesson_route(LessonPlugin& plugin, const net::HttpRequest& req, const std::string& sub,
                                      const host::LogFn& log) {
    const bool state = sub == "/api/state", link = sub == "/api/connect", unlink = sub == "/api/disconnect";
    if (!state && !link && !unlink) return error_response(404, "not_found", "not found");
    if (req.method != (state ? "GET" : "POST")) return error_response(405, "method_not_allowed", "method not allowed");
    if (const auto why = check_mutation(req.method, req.header("host"), req.header("origin"), req.header("content-type"))) {
        if (log) log("warn", "[lesson] rejected " + req.method + " " + sub + ": " + *why);
        return error_response(403, "forbidden", *why);
    }
    if (state) return json_response(200, lesson_shown_json(plugin.shown()));
    if (unlink) {
        plugin.disconnect();
        return json_response(200, jobj({{"ok", jbool(true)}}));
    }
    return connect(plugin, req, log);
}

} // namespace ergo::studio::plugins::lesson
