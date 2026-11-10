// Projects (Cc) and AI instructions (port of studio/routes/projects.ts and usecases/instruct.ts):
//   GET  /api/projects                 POST /api/projects {code, repo_path, project?}
//   GET  /api/templates                POST /api/projects/<code>/instruct {call_name, args, extra_prompt?}
//   GET  /api/runs[?project=]          GET  /api/runs/<id>
#include "delegation_request.h"
#include "studio_deps.h"

namespace ergo::studio::plugins::studio {

std::string str_field(const JsonValue& v, const char* key) {
    const JsonValue* x = field(v, key);
    return x && x->is_string() ? x->s : std::string();
}

std::string Route::query(const char* k) const {
    const auto it = req.query.find(k);
    return it == req.query.end() ? std::string() : it->second;
}

std::variant<JsonValue, net::HttpResponse> find_project(StudioDeps& d, const std::string& code) {
    const auto r = d.cc->projects();
    if (!r.ok) return send_error(502, "Concordia: " + r.message);
    for (const auto& p : *r.data.a) {
        if (str_field(p, "code") == code) return p;
    }
    return send_error(404, "プロジェクト " + code + " は Cc に登録されていません");
}

namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

/// Look up the project and template again right before sending (ST-INV-04).
net::HttpResponse instruct(StudioDeps& d, const std::string& code, const InstructionInput& in) {
    auto project = find_project(d, code);
    if (auto* e = std::get_if<net::HttpResponse>(&project)) return *e;
    const auto templates = d.cc->templates();
    if (!templates.ok) return send_error(502, "Concordia: " + templates.message);
    const JsonValue* tpl = nullptr;
    for (const auto& t : *templates.data.a) {
        if (str_field(t, "call_name") == in.call_name) tpl = &t;
    }
    if (!tpl) return send_error(404, "テンプレート " + in.call_name + " がありません (無効化を含む)");
    const auto built = build_invoke_request(std::get<JsonValue>(project), *tpl, in, d.cfg.triggered_by);
    if (!built.ok) return send_error(400, built.reason);
    const auto r = d.cc->invoke(built.request);
    if (!r.ok) return send_error(r.status && r.status < 500 ? r.status : 502, "Concordia: " + r.message);
    return json_response(201, jobj({{"run", r.data}}));
}

} // namespace

net::HttpResponse implement_task(StudioDeps& d, const std::string& code, const std::string& task_id,
                                 const std::string& call_name, const std::string& extra) {
    const auto task = d.actio->task(task_id);
    if (!task.ok) return send_error(task.status == 404 ? 404 : 502, "Actio: " + task.message);
    InstructionInput in;
    in.call_name     = call_name;
    in.task_text     = task_prompt(str_field(task.data, "id"), str_field(task.data, "title"), str_field(task.data, "description"));
    in.extra_prompt  = extra;
    in.actio_task_id = str_field(task.data, "id");
    return instruct(d, code, in);
}

std::optional<net::HttpResponse> project_routes(StudioDeps& d, const Route& r) {
    const auto& s = r.seg;
    if (s.size() < 2 || s[0] != "api") return std::nullopt;
    if (s.size() == 2 && s[1] == "projects") {
        if (r.is("GET")) {
            const auto res = d.cc->projects();
            return send_result("concordia", res, jobj({{"projects", res.data}}));
        }
        if (r.is("POST")) {
            const std::string code = trim(str_field(r.body, "code")), repo = trim(str_field(r.body, "repo_path"));
            if (code.empty() || repo.empty()) return send_error(400, "code と repo_path は必須です");
            JsonValue in = jobj({{"code", jstr(code)}, {"repo_path", jstr(repo)}, {"added_by", jstr(d.cfg.triggered_by)}});
            if (const std::string p = trim(str_field(r.body, "project")); !p.empty()) in.set("project", jstr(p));
            const auto res = d.cc->register_project(in);
            return send_result("concordia", res, jobj({{"registered", res.data}}));
        }
    }
    if (s.size() == 2 && s[1] == "templates" && r.is("GET")) {
        const auto res = d.cc->templates();
        return send_result("concordia", res, jobj({{"templates", res.data}}));
    }
    if (s.size() == 4 && s[1] == "projects" && s[3] == "instruct" && r.is("POST")) {
        InstructionInput in;
        in.call_name = str_field(r.body, "call_name");
        if (in.call_name.empty()) return send_error(400, "call_name は必須です");
        if (const JsonValue* a = field(r.body, "args"); a && a->is_object()) in.args = *a;
        in.extra_prompt = str_field(r.body, "extra_prompt");
        return instruct(d, s[2], in);
    }
    if (s.size() == 2 && s[1] == "runs" && r.is("GET")) {
        std::string repo;
        if (const std::string code = trim(r.query("project")); !code.empty()) {
            auto p = find_project(d, code);
            if (auto* e = std::get_if<net::HttpResponse>(&p)) return *e;
            repo = str_field(std::get<JsonValue>(p), "repo_path");
        }
        const auto res = d.cc->runs(100);
        JsonValue mine = JsonValue::make_array();
        if (res.ok) {
            for (const auto& run : *res.data.a) {
                if (str_field(run, "triggered_by") == d.cfg.triggered_by && (repo.empty() || run_belongs_to(run, repo))) mine.push(run);
            }
        }
        return send_result("concordia", res, jobj({{"runs", mine}}));
    }
    if (s.size() == 3 && s[1] == "runs" && r.is("GET")) {
        const auto res = d.cc->run(s[2]);
        return send_result("concordia", res, jobj({{"run", res.data}}));
    }
    return std::nullopt;
}

} // namespace ergo::studio::plugins::studio
