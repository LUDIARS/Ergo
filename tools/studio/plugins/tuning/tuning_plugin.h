#pragma once
/// "tuning" plugin: a parameter window for every value a running game publishes
/// through ergo_bind (the variable plugin holds them; the tab edits them there,
/// grouped by category: "look/...", "balance/...", ...), and versions of those
/// values: saved with a name and a note, listed, compared with the live values
/// and restored. Versions live in the folder the workspace profile gives the
/// app (a game repository can keep them), else <studio home>/tuning/<app>.
///   GET  /api/health
///   GET  /api/versions?app=<app>              {folder, versions: [{id, name, note, created, values}]}
///   GET  /api/versions/<app>/<id>             the version file
///   POST /api/versions                        {app, name, note, values}: save a new version
#include "tuning_store.h"
#include "../common/workspace_profiles.h"

#include "ergo/studio/host/plugin.h"

namespace ergo::studio::plugins::tuning {

class TuningPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    TuningPlugin(TuningStore store, std::string home, std::string plugin_dirs, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

private:
    std::filesystem::path folder_of(const std::string& app) const;
    net::HttpResponse     save(const net::HttpRequest& req);

    host::PluginInfo info_;
    TuningStore      store_;
    std::string      home_;
    std::string      plugin_dirs_;
    host::LogFn      log_;
};

std::unique_ptr<host::IPlugin> make_tuning_plugin(const std::string& studio_home, const std::string& plugin_dirs,
                                                  const host::LogFn& log);

} // namespace ergo::studio::plugins::tuning
