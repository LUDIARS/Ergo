#include "terpsichore_plugin.h"

namespace ergo::studio::plugins::musa_terpsichore {
namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

/// The command server takes no WebSocket connections.
class NoSockets final : public net::IWsAcceptor {
public:
    bool            accepts(const std::string&) const override { return false; }
    net::WsHandlers accept(const std::string&, const net::HttpRequest&, std::shared_ptr<net::IWsConnection>) override {
        return {};
    }
};

NoSockets& no_sockets() {
    static NoSockets s;
    return s;
}

} // namespace

TerpsichorePlugin::TerpsichorePlugin(std::filesystem::path config_file, host::LogFn log)
    : info_{"musa_terpsichore", "Terpsichore (MUSA)",
            "Ars-Musa Terpsichore: configure, start and stop the external command server.", "\xF0\x9F\x8E\xAE"},
      file_(std::move(config_file)),
      log_(std::move(log)) {
    // config_error_ is set only for a real problem (no home / broken file);
    // a missing file is the normal "not configured yet" state.
    auto loaded   = load_config(file_);
    config_       = std::move(loaded.config);
    config_error_ = file_.empty() ? std::string("config path is not known (no home directory)") : loaded.error;
}

TerpsichorePlugin::~TerpsichorePlugin() { stop(); }

std::optional<TerpsichoreConfig> TerpsichorePlugin::config() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return config_;
}

bool TerpsichorePlugin::update_config(const TerpsichoreConfig& c, std::string& err) {
    if (!save_config(file_, c, err)) return false;
    std::lock_guard<std::mutex> lk(mtx_);
    config_ = c;
    config_error_.clear();
    return true;
}

bool TerpsichorePlugin::start(std::string& err) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (server_) { err = "Server already running"; return false; }
    if (!config_) {
        err = config_error_.empty() ? "not configured: save a config with a port first" : config_error_;
        return false;
    }
    if (!config_->enabled) { err = "command server is disabled in the config"; return false; }
    auto api = std::make_unique<CommandApi>(*config_);
    if (handler_) api->set_command_handler(handler_);
    auto server = std::make_unique<net::Server>(net::ServerConfig{config_->host, config_->port}, *api, no_sockets(), log_);
    if (!server->start(err)) return false;
    api_    = std::move(api);
    server_ = std::move(server);
    if (log_) log_("info", "[musa_terpsichore] command server on " + config_->host + ":" + std::to_string(server_->port()));
    return true;
}

void TerpsichorePlugin::stop() {
    std::unique_ptr<net::Server> server;
    std::unique_ptr<CommandApi>  api;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        server = std::move(server_);
        api    = std::move(api_);
    }
    // Stop before the API goes away: connection threads still call it.
    if (server) server->stop();
}

bool TerpsichorePlugin::running() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return server_ != nullptr;
}

JsonValue TerpsichorePlugin::status() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (api_) {
        JsonValue s = api_->status();
        s.set("port", jnum(server_->port()));
        return s;
    }
    return jobj({{"running", jbool(false)}, {"port", jnum(config_ ? config_->port : 0)},
                 {"role", jstr(config_ ? role_name(config_->role) : "")}, {"uptime_seconds", jnum(0)},
                 {"request_count", jnum(0)}});
}

void TerpsichorePlugin::set_command_handler(CommandHandler h) {
    std::lock_guard<std::mutex> lk(mtx_);
    handler_ = std::move(h);
}

net::HttpResponse TerpsichorePlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    const bool get  = req.method == "GET";
    const bool post = req.method == "POST";
    if (sub == "/api/health" && get) return json_response(200, snapshot());
    if (sub == "/api/status" && get) return json_response(200, status());
    if (sub == "/api/config" && get) {
        std::lock_guard<std::mutex> lk(mtx_);
        return json_response(200, jobj({{"config", config_ ? to_json_value(*config_) : JsonValue::make_null()},
                                        {"error", jstr(config_error_)}, {"path", jstr(file_.u8string())}}));
    }
    if (sub == "/api/config" && post) {
        JsonValue body;
        if (!ergo::common::jsonm::parse(req.body, body)) return fail(400, "invalid JSON body");
        const JsonValue* c = field(body, "config");
        std::string err;
        const auto cfg = parse_config(c ? *c : body, err);
        if (!cfg) return fail(400, err);
        if (!update_config(*cfg, err)) return fail(500, err);
        return json_response(200, jobj({{"ok", jbool(true)}, {"config", to_json_value(*cfg)}}));
    }
    if (sub == "/api/start" && post) {
        std::string err;
        if (start(err)) return json_response(200, jobj({{"ok", jbool(true)}, {"status", status()}}));
        return fail(running() ? 409 : 400, err);
    }
    if (sub == "/api/stop" && post) {
        stop();
        return json_response(200, jobj({{"ok", jbool(true)}}));
    }
    return fail(404, "not found");
}

JsonValue TerpsichorePlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    // Unconfigured is normal (the server is opt-in); a broken file or no home is not.
    JsonValue h = jobj({{"ok", jbool(config_error_.empty())}, {"running", jbool(server_ != nullptr)},
                        {"configured", jbool(config_.has_value())}});
    if (!config_error_.empty()) h.set("err", jstr(config_error_));
    return h;
}

std::unique_ptr<host::IPlugin> make_terpsichore_plugin(const std::string& home, const host::LogFn& log) {
    return std::make_unique<TerpsichorePlugin>(config_path(home), log);
}

} // namespace ergo::studio::plugins::musa_terpsichore
