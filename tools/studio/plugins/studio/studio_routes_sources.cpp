// Sources (git + Revisor), resources (Curare), plugin packs and connections
// (port of studio/routes/{sources,resources,plugins,connections}.ts):
//   GET  /api/sources   GET /api/sources/<code>   POST /api/sources/<code>/fetch|fast-forward
//   GET  /api/resources/projects[/<id>/folders[/<fid>/assets]]   GET /api/resources/assets/<id>/versions
//   POST /api/resources/sync {code, subdir, versionIds}
//   GET  /api/plugins   POST /api/plugins/preview|install|enable   DELETE /api/plugins?id=
//   GET  /api/connections   GET /api/endpoints
#include "delegation_request.h"
#include "resource_download.h"
#include "resource_sync.h"
#include "studio_deps.h"

#include <filesystem>
#include <future>

namespace ergo::studio::plugins::studio {
namespace {

std::string utf8(const std::filesystem::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

net::HttpResponse source_detail(StudioDeps& d, const JsonValue& project) {
    const auto slug = repo_slug_from_origin(str_field(project, "repo_origin"));
    std::string st_err, pr_err = "repo origin が未登録です";
    const auto st  = d.git->status(str_field(project, "repo_path"), st_err);
    const auto prs = slug ? d.revisor->prs(*slug, pr_err) : std::nullopt;
    JsonValue ff = JsonValue::make_null();
    if (st) {
        const auto dec = decide_fast_forward(st->branch);
        ff = jobj({{"allowed", jbool(dec.allowed)}});
        if (dec.allowed) ff.set("branch", jstr(dec.branch));
        else ff.set("reason", jstr(dec.reason));
    }
    JsonValue pr_list = JsonValue::make_array();
    if (prs) {
        for (std::size_t i = 0; i < prs->a->size() && i < 50; ++i) pr_list.push((*prs->a)[i]);
    }
    const JsonValue* wf = field(project, "revisor_workflow");
    return json_response(200, jobj({{"code", jstr(str_field(project, "code"))},
                                    {"repository", slug ? jstr(*slug) : JsonValue::make_null()},
                                    {"workflow", wf ? *wf : JsonValue::make_null()},
                                    {"status", st ? to_json_value(*st) : JsonValue::make_null()},
                                    {"statusError", st ? JsonValue::make_null() : jstr(st_err)}, {"fastForward", ff},
                                    {"prs", pr_list}, {"prsError", prs ? JsonValue::make_null() : jstr(pr_err)}}));
}

net::HttpResponse sync_resources(StudioDeps& d, const JsonValue& body) {
    const std::string code = str_field(body, "code");
    std::vector<std::string> ids;
    if (const JsonValue* v = field(body, "versionIds"); v && v->is_array()) {
        for (const auto& x : *v->a) {
            if (x.is_string()) ids.push_back(x.s);
        }
    }
    if (code.empty()) return send_error(400, "code は必須です");
    if (ids.empty() || ids.size() > 500) return send_error(400, "版は 1〜500 件で指定してください");
    auto project = find_project(d, code);
    if (auto* e = std::get_if<net::HttpResponse>(&project)) return *e;
    const auto dest = resolve_inside(std::filesystem::u8path(str_field(std::get<JsonValue>(project), "repo_path")),
                                     str_field(body, "subdir"));
    if (!dest) return send_error(400, "取得先はプロジェクトの repo の中の相対パスにしてください");
    const auto plan = d.cu->plan(ids, local_hashes(*dest));
    if (!plan.ok) return send_error(plan.status && plan.status < 500 ? plan.status : 502, "Curare: " + plan.message);
    // Signed URLs expire in 300 s; each item gets 8x the normal timeout.
    const auto items = execute_plan(plan.data, *dest, *d.http, d.cfg.request_timeout_ms * 8);
    JsonValue arr = JsonValue::make_array();
    for (const auto& i : items) arr.push(to_json_value(i));
    const JsonValue* bytes = field(plan.data, "transferBytes");
    return json_response(200, jobj({{"ok", jbool(true)}, {"destination", jstr(utf8(*dest))},
                                    {"transferBytes", bytes ? *bytes : jnum(0)}, {"items", arr}}));
}

/// Read-only probes of every service (port of usecases/probe-connections.ts).
JsonValue probe_connections(StudioDeps& d) {
    const auto snap = d.endpoints->snapshot(true);
    std::vector<ProbeResult> probes;
    auto record = [&](Service key, const ServiceResult& r, const std::string& auth) {
        ProbeResult p;
        p.key        = key;
        p.url        = snap.urls.count(key) ? snap.urls.at(key) : std::nullopt;
        p.ok         = r.ok;
        if (!r.ok) {
            p.failure = r.kind;
            p.detail  = r.message;
        }
        p.checked_at = now_iso_utc();
        p.auth       = auth;
        std::lock_guard<std::mutex> lk(d.last_ok_mtx);
        if (r.ok) d.last_ok[key] = p.checked_at;
        if (d.last_ok.count(key)) p.last_ok_at = d.last_ok[key];
        probes.push_back(std::move(p));
    };
    ServiceResult ex;
    ex.ok = snap.topology == "ok";
    if (!ex.ok) {
        ex.kind    = snap.topology == "not_configured" ? FailureKind::NotConfigured : FailureKind::Unreachable;
        ex.message = snap.topology == "not_configured" ? "EXCUBITOR_URL も LUDIARS_ROOT の catalog も無く、接続先を解決できません"
                                                       : "Excubitor の topology を取得できません";
    }
    record(Service::Excubitor, ex, "");
    record(Service::Concordia, d.cc->projects(), "loopback");
    record(Service::Actio, d.actio->me(), d.cfg.actio_token ? "Cernere トークン" : "local mode");
    record(Service::Breviarium, d.br->health(), "loopback");
    if (d.cu->has_token()) {
        record(Service::Curare, d.cu->me(), "Cernere トークン");
    } else {
        const bool has_url = snap.urls.count(Service::Curare) && snap.urls.at(Service::Curare);
        record(Service::Curare,
               ServiceResult::fail(has_url ? FailureKind::Unauthorized : FailureKind::NotConfigured,
                                   has_url ? "ERGO_CURARE_TOKEN が未設定です" : "Curare の接続先がありません (ERGO_CURARE_URL)"),
               "未設定");
    }
    const std::string rv = d.revisor->ping();
    ServiceResult rr;
    rr.ok = rv.empty();
    if (!rr.ok) {
        rr.kind    = d.revisor->available() ? FailureKind::ServerError : FailureKind::NotConfigured;
        rr.message = rv;
    }
    record(Service::Revisor, rr, "CLI");
    // Cernere: any HTTP answer means it is up.
    auto cr = d.caller->call(snap.urls.count(Service::Cernere) ? snap.urls.at(Service::Cernere) : std::nullopt, "/");
    if (!cr.ok && cr.kind != FailureKind::Unreachable && cr.kind != FailureKind::Timeout && cr.kind != FailureKind::NotConfigured) cr.ok = true;
    record(Service::Cernere, cr, "");

    JsonValue map = build_connection_map(probes);
    JsonValue pr = JsonValue::make_array();
    for (const auto& p : probes) pr.push(to_json_value(p));
    JsonValue others = JsonValue::make_array();
    for (const auto& [name, url] : snap.all) others.push(jobj({{"name", jstr(name)}, {"url", jstr(url)}}));
    map.set("resolvedAt", jstr(snap.resolved_at));
    map.set("topology", jstr(snap.topology));
    map.set("probes", pr);
    map.set("others", others);
    return map;
}

} // namespace

std::optional<net::HttpResponse> source_routes(StudioDeps& d, const Route& r) {
    const auto& s = r.seg;
    if (s.size() < 2 || s[0] != "api") return std::nullopt;
    if (s[1] == "sources") {
        if (s.size() == 2 && r.is("GET")) {
            const auto projects = d.cc->projects();
            if (!projects.ok) return send_result("concordia", projects, JsonValue());
            // git status of each project, 4 at a time.
            const auto& list = *projects.data.a;
            std::vector<JsonValue> rows(list.size());
            for (std::size_t base = 0; base < list.size(); base += 4) {
                std::vector<std::future<void>> jobs;
                for (std::size_t i = base; i < list.size() && i < base + 4; ++i) {
                    jobs.push_back(std::async(std::launch::async, [&, i] {
                        std::string err;
                        const auto st = d.git->status(str_field(list[i], "repo_path"), err);
                        JsonValue row = jobj({{"code", jstr(str_field(list[i], "code"))}, {"project", jstr(str_field(list[i], "project"))}});
                        if (st) row.set("status", to_json_value(*st));
                        else row.set("error", jstr(err));
                        rows[i] = std::move(row);
                    }));
                }
                for (auto& j : jobs) j.get();
            }
            JsonValue arr = JsonValue::make_array();
            for (auto& row : rows) arr.push(std::move(row));
            return json_response(200, jobj({{"sources", arr}}));
        }
        if (s.size() >= 3) {
            auto project = find_project(d, s[2]);
            if (auto* e = std::get_if<net::HttpResponse>(&project)) return *e;
            const JsonValue& p    = std::get<JsonValue>(project);
            const std::string repo = str_field(p, "repo_path");
            if (s.size() == 3 && r.is("GET")) return source_detail(d, p);
            if (s.size() == 4 && s[3] == "fetch" && r.is("POST")) {
                const auto g = d.git->fetch(repo);
                return g.ok ? json_response(200, jobj({{"ok", jbool(true)}, {"message", jstr(g.message)}})) : send_error(502, g.message);
            }
            if (s.size() == 4 && s[3] == "fast-forward" && r.is("POST")) {
                // Decide on the state right before acting (the editor's copy may be stale).
                std::string err;
                const auto st = d.git->status(repo, err);
                if (!st) return send_error(502, err);
                const auto dec = decide_fast_forward(st->branch);
                if (!dec.allowed) return send_error(409, dec.reason);
                const auto g = d.git->fast_forward(repo, dec.branch);
                return g.ok ? json_response(200, jobj({{"ok", jbool(true)}, {"message", jstr(g.message)}})) : send_error(502, g.message);
            }
        }
    }
    if (s[1] == "resources") {
        if (s.size() == 3 && s[2] == "projects" && r.is("GET")) {
            const auto res = d.cu->projects();
            return send_result("curare", res, jobj({{"projects", res.data}}));
        }
        if (s.size() == 5 && s[2] == "projects" && s[4] == "folders" && r.is("GET")) {
            const auto res = d.cu->folders(s[3]);
            return send_result("curare", res, jobj({{"folders", res.data}}));
        }
        if (s.size() == 7 && s[2] == "projects" && s[4] == "folders" && s[6] == "assets" && r.is("GET")) {
            const auto res = d.cu->assets(s[3], s[5]);
            return send_result("curare", res, jobj({{"assets", res.data}}));
        }
        if (s.size() == 5 && s[2] == "assets" && s[4] == "versions" && r.is("GET")) {
            const auto res = d.cu->versions(s[3]);
            return send_result("curare", res, jobj({{"versions", res.data}}));
        }
        if (s.size() == 3 && s[2] == "sync" && r.is("POST")) return sync_resources(d, r.body);
    }
    if (s[1] == "plugins") {
        std::string err;
        if (s.size() == 2 && r.is("GET")) {
            JsonValue packs = JsonValue::make_array(), owners = JsonValue::make_array();
            for (const auto& p : d.installer->list()) packs.push(to_json_value(p));
            for (const auto& o : d.cfg.plugin_owners) owners.push(jstr(o));
            return json_response(200, jobj({{"packs", packs}, {"owners", owners}, {"home", jstr(d.cfg.home)}}));
        }
        if (s.size() == 2 && r.is("DELETE")) {
            const std::string id = r.query("id");
            if (id.empty()) return send_error(400, "id は必須です");
            if (!d.installer->remove(id, err)) return send_error(404, err);
            return json_response(200, jobj({{"removed", jstr(id)}, {"restartRequired", jbool(true)}}));
        }
        if (s.size() == 3 && s[2] == "preview" && r.is("POST")) {
            const auto parsed = parse_plugin_source(str_field(r.body, "source"), d.cfg.plugin_owners);
            if (!parsed.source) return send_error(400, parsed.reason);
            const auto pv = d.installer->preview(*parsed.source, err);
            return pv ? json_response(200, jobj({{"preview", to_json_value(*pv)}})) : send_error(502, err);
        }
        if (s.size() == 3 && s[2] == "install" && r.is("POST")) {
            const std::string token = str_field(r.body, "token");
            if (token.empty()) return send_error(400, "token は必須です");
            const auto pack = d.installer->install(token, err);
            return pack ? json_response(201, jobj({{"pack", to_json_value(*pack)}, {"restartRequired", jbool(true)}}))
                        : send_error(409, err);
        }
        if (s.size() == 3 && s[2] == "enable" && r.is("POST")) {
            const std::string id = str_field(r.body, "id");
            const JsonValue*  en = field(r.body, "enabled");
            if (id.empty() || !en || !en->is_bool()) return send_error(400, "id と enabled は必須です");
            const auto pack = d.installer->set_enabled(id, en->b, err);
            return pack ? json_response(200, jobj({{"pack", to_json_value(*pack)}, {"restartRequired", jbool(true)}}))
                        : send_error(404, err);
        }
    }
    if (s.size() == 2 && s[1] == "connections" && r.is("GET")) return json_response(200, probe_connections(d));
    if (s.size() == 2 && s[1] == "endpoints" && r.is("GET")) {
        const auto snap = d.endpoints->snapshot();
        JsonValue urls = JsonValue::make_object();
        for (const auto& [k, v] : snap.urls) urls.set(service_key(k), v ? jstr(*v) : JsonValue::make_null());
        return json_response(200, jobj({{"resolvedAt", jstr(snap.resolved_at)}, {"topology", jstr(snap.topology)}, {"urls", urls}}));
    }
    return std::nullopt;
}

} // namespace ergo::studio::plugins::studio
