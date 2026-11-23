#include "services_plugin.h"

#include "../common/json_parse.h"
#include "../common/json_util.h"

#include <algorithm>
#include <cctype>

namespace ergo::studio::plugins::services {

namespace {

// The Studio announces its own tests under one name: Concordia's claim is per session.
constexpr const char* kClaimSession = "ergo-studio";

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

std::string text_of(const JsonValue& v, const char* key) {
    const JsonValue* f = field(v, key);
    return f != nullptr && f->is_string() ? f->as_string() : std::string();
}

bool valid_code(const std::string& code) {
    return !code.empty() && code.size() <= 64 &&
           std::all_of(code.begin(), code.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; });
}

} // namespace

JsonValue claim_body(const std::string& session_id, const std::string& code, const std::string& action) {
    return jobj({{"session_id", jstr(session_id)},
                 {"service", jstr(code)},
                 {"note", jstr("Ergo Studio から " + action + " (サービスタブ)")}});
}

JsonValue pick_services(const JsonValue& excubitor_list, const std::vector<WorkspaceProfile>& profiles) {
    const JsonValue* list = excubitor_list.is_array() ? &excubitor_list : field(excubitor_list, "services");
    JsonValue out = JsonValue::make_array();
    if (list == nullptr || !list->is_array()) return out;
    bool any_listed = false;
    for (const auto& p : profiles) any_listed = any_listed || !p.services.empty();
    for (const JsonValue& s : *list->a) {
        const std::string code = text_of(s, "code");
        if (code.empty()) continue;
        std::string profile;
        for (const auto& p : profiles) {
            if (std::find(p.services.begin(), p.services.end(), code) != p.services.end()) profile = p.name;
        }
        if (any_listed && profile.empty()) continue;
        const JsonValue* running = field(s, "running");
        const JsonValue* port = field(s, "port");
        JsonValue item = jobj({{"code", jstr(code)},
                               {"name", jstr(text_of(s, "name"))},
                               {"state", jstr(text_of(s, "state"))},
                               {"running", jbool(running != nullptr && running->as_bool())},
                               {"project", jstr(text_of(s, "project_code"))},
                               {"profile", jstr(profile)}});
        if (port != nullptr && port->is_number()) item.set("port", *port);
        out.push(std::move(item));
    }
    return out;
}

ServicesPlugin::ServicesPlugin(studio::StudioConfig cfg, std::string plugin_dirs,
                               std::shared_ptr<platform::IHttpClient> http, host::LogFn log)
    : info_{"services", "サービス", "ゲームのサーバーやツールを Excubitor 経由で起動・停止する (起動時は Concordia にテストを一報)。",
            "\xE2\x96\xB6"},
      cfg_(std::move(cfg)),
      plugin_dirs_(std::move(plugin_dirs)),
      caller_(std::make_unique<studio::ServiceCaller>(std::move(http), cfg_.request_timeout_ms)),
      endpoints_(std::make_unique<studio::EndpointResolver>(cfg_, *caller_)),
      log_(std::move(log)) {}

ProfileSet ServicesPlugin::profiles() const {
    return load_workspace_profiles(std::filesystem::u8path(cfg_.home), plugin_dirs_, &process_env);
}

bool ServicesPlugin::offered(const std::string& code) const {
    const ProfileSet set = profiles();
    bool any_listed = false;
    for (const auto& p : set.profiles) {
        if (std::find(p.services.begin(), p.services.end(), code) != p.services.end()) return true;
        any_listed = any_listed || !p.services.empty();
    }
    return !any_listed;
}

JsonValue ServicesPlugin::snapshot() const {
    const ProfileSet set = profiles();
    const bool excubitor = endpoints_->url(studio::Service::Excubitor).has_value();
    JsonValue errors = JsonValue::make_array();
    for (const auto& e : set.errors) errors.push(jstr(e));
    JsonValue names = JsonValue::make_array();
    for (const auto& p : set.profiles) names.push(jstr(p.name));
    JsonValue out = jobj({{"ok", jbool(excubitor)},
                          {"excubitor", jbool(excubitor)},
                          {"concordia", jbool(endpoints_->url(studio::Service::Concordia).has_value())},
                          {"profiles", names},
                          {"profile_errors", errors}});
    if (!excubitor) out.set("err", jstr("Excubitor の接続先が分かりません (EXCUBITOR_URL / LUDIARS_ROOT)"));
    return out;
}

net::HttpResponse ServicesPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/services" && req.method == "GET") return list();
    const std::string prefix = "/api/services/";
    if (sub.rfind(prefix, 0) == 0) {
        const std::string rest = sub.substr(prefix.size());
        const std::size_t slash = rest.find('/');
        const std::string code = rest.substr(0, slash);
        const std::string what = slash == std::string::npos ? std::string() : rest.substr(slash + 1);
        if (!valid_code(code)) return fail(400, "bad service code");
        if (what == "control" && req.method == "POST") return control(code, req);
        if (what == "logs" && req.method == "GET") return logs(code);
    }
    return fail(404, "not found");
}

net::HttpResponse ServicesPlugin::list() {
    const auto result = caller_->call(endpoints_->url(studio::Service::Excubitor), "/api/v1/services");
    if (!result.ok) return fail(studio::failure_http_status(result.kind), "Excubitor: " + result.message);
    return json_response(200, pick_services(result.data, profiles().profiles));
}

net::HttpResponse ServicesPlugin::control(const std::string& code, const net::HttpRequest& req) {
    const auto body = parse_json(req.body);
    if (!body) return fail(400, "body must be JSON");
    const std::string action = text_of(*body, "action");
    if (action != "start" && action != "stop" && action != "restart") return fail(400, "action must be start, stop or restart");
    if (!offered(code)) return fail(403, code + " is not in a workspace profile");
    const JsonValue* force = field(*body, "force");
    const auto concordia = endpoints_->url(studio::Service::Concordia);
    if (action != "stop") {
        // cc-test: announce the test before the service starts; someone else's claim stops us unless forced.
        if (!concordia) return fail(503, "Concordia の接続先が分かりません (テストの一報が出せません)");
        const auto claim = caller_->call(concordia, "/v1/testing/claim", "POST", claim_body(kClaimSession, code, action));
        if (!claim.ok) return fail(studio::failure_http_status(claim.kind), "Concordia: " + claim.message);
        const JsonValue* conflicts = field(claim.data, "conflicts");
        const bool contested = conflicts != nullptr && conflicts->is_array() && !conflicts->a->empty();
        if (contested && !(force != nullptr && force->as_bool())) {
            return json_response(409, jobj({{"ok", jbool(false)},
                                            {"err", jstr(code + " は別の作業がテスト中です")},
                                            {"conflicts", *conflicts}}));
        }
    }
    const auto result = caller_->call(endpoints_->url(studio::Service::Excubitor), "/api/v1/services/" + code + "/control",
                                      "POST", jobj({{"action", jstr(action)}}), {}, cfg_.request_timeout_ms * 15);
    if (action == "stop" && concordia) {
        const auto released = caller_->call(concordia, "/v1/testing/release", "POST",
                                            jobj({{"session_id", jstr(kClaimSession)}, {"service", jstr(code)}}));
        if (!released.ok && log_) log_("warn", "[services] Concordia release failed: " + released.message);
    }
    if (!result.ok) {
        if (log_) log_("error", "[services] " + code + " " + action + " failed: " + result.message);
        return fail(studio::failure_http_status(result.kind), "Excubitor: " + result.message);
    }
    if (log_) log_("info", "[services] " + code + " " + action);
    return json_response(200, jobj({{"ok", jbool(true)}, {"code", jstr(code)}, {"action", jstr(action)}, {"result", result.data}}));
}

net::HttpResponse ServicesPlugin::logs(const std::string& code) {
    const auto result =
        caller_->call(endpoints_->url(studio::Service::Excubitor), "/api/v1/services/" + code + "/logs/recent");
    if (!result.ok) return fail(studio::failure_http_status(result.kind), "Excubitor: " + result.message);
    return json_response(200, result.data);
}

std::unique_ptr<host::IPlugin> make_services_plugin(const studio::StudioConfig& cfg, const std::string& plugin_dirs,
                                                    const host::LogFn& log) {
    return std::make_unique<ServicesPlugin>(cfg, plugin_dirs, platform::make_system_http_client(), log);
}

} // namespace ergo::studio::plugins::services
