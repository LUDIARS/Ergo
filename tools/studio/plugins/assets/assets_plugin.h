#pragma once
/// "assets" plugin: a read-only list of a game's assets, from the folders the
/// workspace profiles name ("assets": [{name, path}]). The web page and any
/// client browse it; nothing is written, moved or deleted.
///   GET /api/health
///   GET /api/roots                           [{index, profile, name, path}]
///   GET /api/list?root=<i>&dir=<rel>         one folder level (asset_catalog.h)
///   GET /api/file?root=<i>&path=<rel>        preview: raster images and small text only
#include "../common/json_util.h"
#include "../common/workspace_profiles.h"

#include "ergo/studio/host/plugin.h"

#include <string>
#include <vector>

namespace ergo::studio::plugins::assets {

class AssetsPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    AssetsPlugin(std::string home, std::string plugin_dirs, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

private:
    struct Root {
        std::string           profile;
        AssetRoot             root;
    };
    std::vector<Root> roots() const;
    net::HttpResponse list(const net::HttpRequest& req) const;
    net::HttpResponse file(const net::HttpRequest& req) const;

    host::PluginInfo info_;
    std::string      home_;
    std::string      plugin_dirs_;
    host::LogFn      log_;
};

std::unique_ptr<host::IPlugin> make_assets_plugin(const std::string& studio_home, const std::string& plugin_dirs,
                                                  const host::LogFn& log);

} // namespace ergo::studio::plugins::assets
