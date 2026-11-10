// Tasks (Actio), project status (Br) and implementing a task (port of studio/routes/tasks.ts):
//   GET   /api/tasks?project=&done=1     POST /api/tasks {title, description?, priority?, projectId?}
//   PATCH /api/tasks/<id>                POST /api/tasks/<id>/implement {code, call_name, extra_prompt?}
//   GET   /api/status   GET /api/status/<code>   POST /api/status/<code>/refresh
#include "studio_deps.h"

#include <set>

namespace ergo::studio::plugins::studio {

net::HttpResponse implement_task(StudioDeps& d, const std::string& code, const std::string& task_id,
                                 const std::string& call_name, const std::string& extra);

namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

/// Actio task input from the body; the error message when invalid.
std::variant<JsonValue, std::string> task_input(const JsonValue& body) {
    static const std::set<std::string> statuses{"open", "in_progress", "blocked", "done", "cancelled"};
    static const std::set<std::string> priorities{"low", "medium", "high", "critical"};   // Actio's VALID_PRIORITIES
    JsonValue out = JsonValue::make_object();
    if (const JsonValue* t = field(body, "title"); t && t->is_string()) out.set("title", jstr(trim(t->s)));
    if (const JsonValue* t = field(body, "description"); t && t->is_string()) out.set("description", *t);
    if (const JsonValue* t = field(body, "status"); t && t->is_string()) {
        if (!statuses.count(t->s)) return std::string("status が不正です");
        out.set("status", *t);
    }
    if (const JsonValue* t = field(body, "priority"); t && t->is_string() && !t->s.empty()) {
        if (!priorities.count(t->s)) return std::string("priority が不正です");
        out.set("priority", *t);
    }
    if (const JsonValue* t = field(body, "projectId"); t && t->is_string() && !trim(t->s).empty()) out.set("projectId", jstr(trim(t->s)));
    return out;
}

} // namespace

std::optional<net::HttpResponse> task_routes(StudioDeps& d, const Route& r) {
    const auto& s = r.seg;
    if (s.size() < 2 || s[0] != "api") return std::nullopt;
    if (s[1] == "tasks") {
        if (s.size() == 2 && r.is("GET")) {
            const auto res = d.actio->tasks(trim(r.query("project")), r.query("done") == "1");
            return send_result("actio", res, jobj({{"tasks", res.data}}));
        }
        if ((s.size() == 2 && r.is("POST")) || (s.size() == 3 && r.is("PATCH"))) {
            if (!r.body.is_object()) return send_error(400, "JSON で送ってください");
            auto in = task_input(r.body);
            if (auto* e = std::get_if<std::string>(&in)) return send_error(400, *e);
            const JsonValue& input = std::get<JsonValue>(in);
            if (s.size() == 2) {
                if (str_field(input, "title").empty()) return send_error(400, "title は必須です");
                const auto res = d.actio->create(input);
                return send_result("actio", res, jobj({{"task", res.data}}));
            }
            const auto res = d.actio->update(s[2], input);
            return send_result("actio", res, jobj({{"task", res.data}}));
        }
        if (s.size() == 4 && s[3] == "implement" && r.is("POST")) {
            const std::string code = trim(str_field(r.body, "code")), call = str_field(r.body, "call_name");
            if (code.empty() || call.empty()) return send_error(400, "code と call_name は必須です");
            return implement_task(d, code, s[2], call, str_field(r.body, "extra_prompt"));
        }
    }
    if (s[1] == "status") {
        if (s.size() == 2 && r.is("GET")) {
            const auto res = d.br->projects();
            return send_result("breviarium", res, jobj({{"projects", res.data}}));
        }
        if (s.size() == 3 && r.is("GET")) {
            const auto res = d.br->overview(s[2]);
            return send_result("breviarium", res, jobj({{"overview", res.data}}));
        }
        if (s.size() == 4 && s[3] == "refresh" && r.is("POST")) {
            const auto res = d.br->refresh(s[2]);
            return send_result("breviarium", res, jobj({{"result", res.data}}));
        }
    }
    return std::nullopt;
}

} // namespace ergo::studio::plugins::studio
