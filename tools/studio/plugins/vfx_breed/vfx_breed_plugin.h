#pragma once
/// "vfx-breed" plugin: evolve a ParticleEffectConfig by picking favourites.
/// Fitness is the human eye; the server is stateless (history lives in the
/// editor). Routes as in the TS plugin:
///   POST /api/generation   parents + selection + direction + strength + seed + count -> children
///   GET  /api/seeds        default effect + the vfx-suggest catalog
///   GET  /api/genes        gene table (id / label / range / scale / wrap)
///   GET  /api/health       advisor status (breeding works without it)
/// The direction advisor is the claude CLI (design J7); when it is not
/// usable only the direction hint is disabled.
#include "../common/claude_cli.h"
#include "breed_direction.h"
#include "breed_ops.h"
#include "breed_request.h"

#include "ergo/studio/host/plugin.h"

namespace ergo::studio::plugins::vfx_breed {

struct VfxBreedSettings {
    std::string backend = "cli";     ///< ERGO_VFX_BACKEND (shared with vfx-suggest)
    std::string model   = "haiku";   ///< ERGO_VFX_MODEL
    std::string bin     = "claude";  ///< CLAUDE_BIN
};

struct GenerationResult {
    std::vector<Child> children;
    double             seed       = 0;
    int                generation = 0;
    std::string        direction_status = "none";   ///< none | applied | unavailable | failed
    DirectionWeights   weights;
    std::string        reason;
};
JsonValue to_json_value(const GenerationResult& r);

class VfxBreedPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    VfxBreedPlugin(VfxBreedSettings s, std::shared_ptr<platform::IProcessRunner> runner, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

    /// Ask the advisor (only when a direction is given), then breed.
    GenerationResult run_generation(const GenerationRequest& req) const;

private:
    std::string advisor_reason() const;   ///< "" when usable
    JsonValue   advisor_health() const;

    host::PluginInfo info_;
    VfxBreedSettings settings_;
    ClaudeCli        cli_;
    host::LogFn      log_;
};

std::unique_ptr<host::IPlugin> make_vfx_breed_plugin(VfxBreedSettings s, const host::LogFn& log);

} // namespace ergo::studio::plugins::vfx_breed
