#pragma once
/// "musa_terpsichore" plugin: manage the Terpsichore command server from the
/// Studio (port of Ars-Musa terpsichore commands/server.rs). Studio-side API:
///   GET  /api/config            current config (or why there is none)
///   POST /api/config  {config}  validate, save to config.json, keep in memory
///   GET  /api/status            {running, port, role, uptime_seconds, request_count}
///   POST /api/start             bind host:port and serve (409 when running)
///   POST /api/stop              stop serving (the original only flipped a flag)
///   GET  /api/health
#include "command_api.h"

#include "ergo/studio/host/plugin.h"

#include <memory>
#include <mutex>

namespace ergo::studio::plugins::musa_terpsichore {

class TerpsichorePlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    TerpsichorePlugin(std::filesystem::path config_file, host::LogFn log);
    ~TerpsichorePlugin() override;

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

    // ---- In-process API ----
    std::optional<TerpsichoreConfig> config() const;
    bool      update_config(const TerpsichoreConfig& c, std::string& err);
    bool      start(std::string& err);
    void      stop();
    bool      running() const;
    JsonValue status() const;
    /// Handler for /api/execute-command; applies to the next start().
    void      set_command_handler(CommandHandler h);

private:
    host::PluginInfo                 info_;
    std::filesystem::path            file_;
    host::LogFn                      log_;
    mutable std::mutex               mtx_;
    std::optional<TerpsichoreConfig> config_;
    std::string                      config_error_;
    CommandHandler                   handler_;
    std::unique_ptr<CommandApi>      api_;      // alive while serving
    std::unique_ptr<net::Server>     server_;
};

std::unique_ptr<host::IPlugin> make_terpsichore_plugin(const std::string& home, const host::LogFn& log);

} // namespace ergo::studio::plugins::musa_terpsichore
