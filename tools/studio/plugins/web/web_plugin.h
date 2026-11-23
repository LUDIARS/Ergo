#pragma once
/// "web" plugin: the Studio in a browser (spec/feature/tool/web.md). One page,
/// compiled into the executable, that works over the other plugins' own API:
/// asset list (assets), lighting (variable + lookdev), every published value
/// and its versions (variable + tuning), and the game: Excubitor services
/// (services) beside the remote play page of the Odeum runner (an iframe of
/// ERGO_STUDIO_PLAY_URL: build, run and WebRTC play live there).
///   GET /            the page (also /index.html)
///   GET /app.js, /style.css
///   GET /api/config  {play_url, public_hosts}
///   GET /api/health
/// The page talks only to its own origin (and frames the play page); the
/// listener's guard decides who may reach it (request_guard.h).
#include "../common/json_util.h"

#include "ergo/studio/host/plugin.h"

#include <string>
#include <vector>

namespace ergo::studio::plugins::web {

struct WebConfig {
    std::string              play_url;       ///< ERGO_STUDIO_PLAY_URL ("" = no game frame)
    std::vector<std::string> public_hosts;   ///< ERGO_STUDIO_PUBLIC_HOSTS (shown on the page)
};

/// Content-Security-Policy of the page: own origin only, plus the play page's
/// origin as a frame source when one is configured.
std::string page_csp(const std::string& play_url);

/// "scheme://host[:port]" of an http(s) URL, or "" when it is not one.
std::string url_origin(const std::string& url);

class WebPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    explicit WebPlugin(WebConfig cfg);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

private:
    net::HttpResponse page_file(const char* body, std::size_t size, const char* type) const;

    host::PluginInfo info_;
    WebConfig        cfg_;
};

std::unique_ptr<host::IPlugin> make_web_plugin(const WebConfig& cfg, const host::LogFn& log);

} // namespace ergo::studio::plugins::web
