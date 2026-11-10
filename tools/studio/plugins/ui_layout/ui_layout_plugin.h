#pragma once
/// "ui_layout" plugin: file I/O for *.uilayout.json documents and the bridge
/// to the running game's UI layout endpoint (Custos). Routes as in the TS
/// plugin:
///   GET  /api/health
///   GET  /api/bridge/config          POST /api/bridge/config {patchUrl?, pullUrl?}
///   GET  /api/file/open?path=        POST /api/file/save {path, json}
///   GET  /api/file/svg?path=         (vector thumbnails)
///   POST /api/bridge/patch  body = patch JSON  -> POST patchUrl, broadcast {op:"patch"}
///   POST /api/bridge/pull                      -> GET pullUrl,  broadcast {op:"document"}
///   WS   /ui_layout/ws  relays {op:"patch"|"document"} frames to every client
/// Differences from the TS plugin: no built-in bridge URLs (the old default
/// pointed at a fixed local port) — the bridge refuses until configured; the
/// file root is given by the caller instead of the working directory.
#include "../common/client_set.h"
#include "../common/json_util.h"

#include "ergo/studio/host/plugin.h"
#include "ergo/studio/platform/http_client.h"

#include <filesystem>
#include <mutex>

namespace ergo::studio::plugins::ui_layout {

/// http:// or https:// with a host. Bridge URLs must pass this.
bool is_http_url(const std::string& url);

class UiLayoutPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                             public host::IPluginHealth {
public:
    /// `root` empty = file APIs answer 503.
    UiLayoutPlugin(std::filesystem::path root, std::shared_ptr<platform::IHttpClient> http, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    /// Broadcast a patch / document frame to every WS client (the view uses it too).
    void broadcast(const std::string& op, const std::string& payload);

private:
    net::HttpResponse open_file(const net::HttpRequest& req, const char* ext, bool svg);
    net::HttpResponse save_file(const net::HttpRequest& req);
    net::HttpResponse bridge_patch(const net::HttpRequest& req);
    net::HttpResponse bridge_pull();
    JsonValue         bridge_json() const;

    host::PluginInfo                       info_;
    std::filesystem::path                  root_;
    std::shared_ptr<platform::IHttpClient> http_;
    host::LogFn                            log_;
    ClientSet                              clients_;
    mutable std::mutex                     mtx_;
    std::string                            patch_url_;
    std::string                            pull_url_;
};

std::unique_ptr<host::IPlugin> make_ui_layout_plugin(const std::filesystem::path& root, const host::LogFn& log);

} // namespace ergo::studio::plugins::ui_layout
