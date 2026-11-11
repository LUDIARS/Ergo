#include "studio_plugin.h"

#include "../common/mutation_guard.h"

namespace ergo::studio::plugins::studio {

std::unique_ptr<StudioDeps> StudioDeps::create(StudioConfig cfg, std::shared_ptr<platform::IHttpClient> http,
                                               std::shared_ptr<platform::IProcessRunner> runner,
                                               const std::filesystem::path& sdk_dir) {
    auto d       = std::make_unique<StudioDeps>();
    d->cfg       = std::move(cfg);
    d->http      = std::move(http);
    const int t  = d->cfg.request_timeout_ms;
    d->caller    = std::make_unique<ServiceCaller>(d->http, t);
    d->endpoints = std::make_unique<EndpointResolver>(d->cfg, *d->caller);
    d->cc        = std::make_unique<ConcordiaClient>(*d->endpoints, *d->caller);
    d->actio     = std::make_unique<ActioClient>(*d->endpoints, *d->caller, d->cfg.actio_token);
    d->br        = std::make_unique<BreviariumClient>(*d->endpoints, *d->caller);
    d->cu        = std::make_unique<CurareClient>(*d->endpoints, *d->caller, d->cfg.curare_token);
    d->git       = std::make_unique<GitClient>(runner, t);
    d->revisor   = std::make_unique<RevisorClient>(runner, d->cfg.ludiars_root, t);
    d->installer = std::make_unique<PluginInstaller>(d->cfg.home.empty() ? std::filesystem::path() : std::filesystem::u8path(d->cfg.home),
                                                     sdk_dir, runner, t);
    return d;
}

StudioPlugin::StudioPlugin(std::unique_ptr<StudioDeps> deps, host::LogFn log)
    : info_{"studio", "Ergo Studio",
            "LUDIARS のサービスと連携して、ゲームを AI と作る (プロジェクト・タスク・ソース・リソース・プラグイン・連携)。",
            "\xF0\x9F\x8E\xAE"},
      deps_(std::move(deps)),
      log_(std::move(log)) {}

net::HttpResponse StudioPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub.rfind("/api/", 0) == 0) {
        if (const auto why = check_mutation(req.method, req.header("host"), req.header("origin"), req.header("content-type"))) {
            if (log_) log_("warn", "[studio] rejected " + req.method + " " + sub + ": " + *why);
            return json_response(403, jobj({{"error", jstr("forbidden")}, {"message", jstr(*why)}}));
        }
    }
    if (sub == "/api/health" && req.method == "GET") return json_response(200, snapshot());
    if (sub == "/api/config" && req.method == "GET") {
        JsonValue owners = JsonValue::make_array();
        for (const auto& o : deps_->cfg.plugin_owners) owners.push(jstr(o));
        return json_response(200, jobj({{"home", jstr(deps_->cfg.home)}, {"actioAuth", jstr(deps_->cfg.actio_mode())},
                                        {"curareToken", jbool(deps_->cfg.curare_token.has_value())}, {"pluginOwners", owners},
                                        {"ludiarsRoot", deps_->cfg.ludiars_root ? jstr(*deps_->cfg.ludiars_root) : JsonValue::make_null()}}));
    }
    Route r{req, {}, JsonValue::make_null()};
    std::size_t start = 1;
    while (start <= sub.size()) {
        const auto end = sub.find('/', start);
        const std::string part = sub.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!part.empty()) r.seg.push_back(part);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (!req.body.empty()) ergo::common::jsonm::parse(req.body, r.body);
    for (auto group : {&project_routes, &task_routes, &source_routes}) {
        if (auto res = group(*deps_, r)) return *res;
    }
    return send_error(404, "not found: " + req.method + " " + sub);
}

JsonValue StudioPlugin::snapshot() const {
    return jobj({{"ok", jbool(true)}, {"actioAuth", jstr(deps_->cfg.actio_mode())},
                 {"curareToken", jbool(deps_->cfg.curare_token.has_value())}});
}

std::unique_ptr<host::IPlugin> make_studio_plugin(const StudioConfig& cfg, const std::filesystem::path& sdk_dir,
                                                  const host::LogFn& log) {
    return std::make_unique<StudioPlugin>(
        StudioDeps::create(cfg, platform::make_system_http_client(), platform::make_system_process_runner(), sdk_dir), log);
}

} // namespace ergo::studio::plugins::studio
