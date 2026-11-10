#pragma once
/// "vfx-suggest" plugin: ParticleEffectConfig presets from game + scene via
/// the claude CLI. Routes as in the TS plugin:
///   GET  /api/games                 GET /api/catalog[?game=]
///   POST /api/generate {game, scene, count?}   (1..4, default 3)
///   POST /api/ks-emitter[?key=] body = config (merged with the defaults)
///   GET  /api/health
/// Only the CLI backend exists (design J7); ERGO_VFX_BACKEND=sdk is reported
/// as unsupported instead of being switched to the CLI silently.
#include "../common/claude_cli.h"
#include "vfx_catalog.h"

#include "ergo/studio/host/plugin.h"

namespace ergo::studio::plugins::vfx_suggest {

struct VfxSuggestSettings {
    std::string backend = "cli";     ///< ERGO_VFX_BACKEND
    std::string model   = "haiku";   ///< ERGO_VFX_MODEL
    std::string bin     = "claude";  ///< CLAUDE_BIN
};

class VfxSuggestPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    VfxSuggestPlugin(VfxSuggestSettings s, std::shared_ptr<platform::IProcessRunner> runner, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

    struct Generated {
        bool                   ok     = false;
        int                    status = 200;
        std::string            error;
        std::vector<VfxPreset> presets;
    };
    /// Blocking LLM call (seconds). `count` is clamped to 1..4.
    Generated generate(const std::string& game, const std::string& scene, int count) const;

private:
    std::string not_ready_reason() const;

    host::PluginInfo   info_;
    VfxSuggestSettings settings_;
    ClaudeCli          cli_;
    host::LogFn        log_;
};

std::unique_ptr<host::IPlugin> make_vfx_suggest_plugin(VfxSuggestSettings s, const host::LogFn& log);

} // namespace ergo::studio::plugins::vfx_suggest
