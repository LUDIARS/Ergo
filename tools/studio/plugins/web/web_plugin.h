#pragma once
/// "web" plugin: the Studio in a browser (spec/feature/tool/web.md). One page,
/// compiled into the executable, that works over the other plugins' own API:
/// asset list (assets), lighting (variable + lookdev), every published value
/// and its versions (variable + tuning), and the game: Excubitor services
/// (services) and remote play of the profiles' games (play: build, run, stop,
/// WebRTC video and input, spec/feature/tool/play.md).
///   GET /            the page (also /index.html)
///   GET /app.js, /style.css
///   GET /api/config  {public_hosts}
///   GET /api/health
/// The page talks only to its own origin (WebRTC media aside); the
/// listener's guard decides who may reach it (request_guard.h).
#include "../common/json_util.h"

#include "ergo/studio/host/plugin.h"

#include <string>
#include <vector>

namespace ergo::studio::plugins::web {

struct WebConfig {
    std::vector<std::string> public_hosts;   ///< ERGO_STUDIO_PUBLIC_HOSTS (shown on the page)
};

/// Content-Security-Policy of the page: own origin only, no frames; video
/// from a MediaStream (blob:).
std::string page_csp();

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
