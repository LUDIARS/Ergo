#pragma once
/// "lookdev" plugin: tune a running game's lighting from the Studio and share
/// the result. The game publishes its look settings through ergo_bind as
/// variables under the category "look/..." (the variable plugin holds them;
/// the look-dev tab edits them there). This plugin keeps what lives outside
/// the game:
///   GET  /api/health              ready state, folders, Concordia
///   GET  /api/presets             saved presets, newest first
///   GET  /api/presets/<name>      {name, text}
///   POST /api/presets             {name, text}: save
///   POST /api/capture-dir         {dir}: a new folder for the game to write one still into
///   POST /api/post                {png, comment, changes[], app}: post the still and its
///                                 preset (<png>.look) to Concordia's 報告 (Discord / Slack)
/// The conventions the game follows (look.apply, look.capture_request, ...)
/// are in spec/feature/tool/lookdev.md.
#include "lookdev_post.h"
#include "lookdev_store.h"

#include "ergo/studio/host/plugin.h"

#include <memory>

namespace ergo::studio::plugins::lookdev {

class LookdevPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    LookdevPlugin(LookdevStore store, studio::StudioConfig cfg, std::shared_ptr<platform::IHttpClient> http,
                  host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

private:
    net::HttpResponse save_preset(const net::HttpRequest& req);
    net::HttpResponse capture_dir();
    net::HttpResponse post_capture(const net::HttpRequest& req);

    host::PluginInfo                         info_;
    LookdevStore                             store_;
    studio::StudioConfig                     cfg_;
    std::unique_ptr<studio::ServiceCaller>   caller_;
    std::unique_ptr<studio::EndpointResolver> endpoints_;
    LookdevPoster                            poster_;
    host::LogFn                              log_;
};

/// Presets in <studio home>/lookdev/presets, captures in <temp>/ergo-studio-lookdev.
std::unique_ptr<host::IPlugin> make_lookdev_plugin(const studio::StudioConfig& cfg, const host::LogFn& log);

} // namespace ergo::studio::plugins::lookdev
