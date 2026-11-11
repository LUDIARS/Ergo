#pragma once
/// "services" plugin: start, stop and restart the LUDIARS services a game
/// workspace runs (its game server, its tools) through Excubitor, from the
/// Studio. Starting announces a test to Concordia first (the same claim the
/// cc-test rule asks of people and sessions) and stopping releases it, so the
/// traffic control knows what is under test. Which services are offered comes
/// from the workspace profiles (common/workspace_profiles.h); without a
/// profile every Excubitor service is listed.
///   GET  /api/health                       Excubitor and Concordia reachable, profiles read
///   GET  /api/services                     [{code, name, state, running, port, project, profile}]
///   POST /api/services/<code>/control      {action: start|stop|restart, force?}: a claim held by
///                                          someone else refuses a start unless `force`
///   GET  /api/services/<code>/logs         Excubitor's recent log lines
#include "../common/workspace_profiles.h"
#include "../studio/endpoints.h"

#include "ergo/studio/host/plugin.h"

#include <memory>

namespace ergo::studio::plugins::services {

/// The claim sent before starting `code` (pure, for tests).
JsonValue claim_body(const std::string& session_id, const std::string& code, const std::string& action);

/// Excubitor's service list cut to `allowed` (empty: all), each tagged with its profile.
JsonValue pick_services(const JsonValue& excubitor_list, const std::vector<WorkspaceProfile>& profiles);

class ServicesPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    ServicesPlugin(studio::StudioConfig cfg, std::string plugin_dirs, std::shared_ptr<platform::IHttpClient> http,
                   host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

private:
    net::HttpResponse list();
    net::HttpResponse control(const std::string& code, const net::HttpRequest& req);
    net::HttpResponse logs(const std::string& code);
    ProfileSet        profiles() const;
    bool              offered(const std::string& code) const;

    host::PluginInfo                          info_;
    studio::StudioConfig                      cfg_;
    std::string                               plugin_dirs_;
    std::unique_ptr<studio::ServiceCaller>    caller_;
    std::unique_ptr<studio::EndpointResolver> endpoints_;
    host::LogFn                               log_;
};

std::unique_ptr<host::IPlugin> make_services_plugin(const studio::StudioConfig& cfg, const std::string& plugin_dirs,
                                                    const host::LogFn& log);

} // namespace ergo::studio::plugins::services
