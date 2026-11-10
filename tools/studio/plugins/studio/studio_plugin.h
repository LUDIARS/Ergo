#pragma once
/// "studio" plugin: make games with AI through the LUDIARS services
/// (projects / tasks / sources / resources / plugins / connections), port of
/// tools/ergo plugins/studio. Spec: spec/feature/tool/studio.md (ST-INV-01..09).
/// Every non-GET call passes check_mutation() first (ST-INV-03).
#include "studio_deps.h"

#include "ergo/studio/host/plugin.h"

namespace ergo::studio::plugins::studio {

class StudioPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    StudioPlugin(std::unique_ptr<StudioDeps> deps, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

    StudioDeps& deps() { return *deps_; }

private:
    host::PluginInfo            info_;
    std::unique_ptr<StudioDeps> deps_;
    host::LogFn                 log_;
};

/// `sdk_dir` = <exe dir>/sdk. The real HTTP client and process runner are used.
std::unique_ptr<host::IPlugin> make_studio_plugin(const StudioConfig& cfg, const std::filesystem::path& sdk_dir,
                                                  const host::LogFn& log);

} // namespace ergo::studio::plugins::studio
