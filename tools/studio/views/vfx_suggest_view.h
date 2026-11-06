#pragma once
/// Native tab of "vfx-suggest": pick a game, describe the scene, generate
/// ParticleEffectConfig presets with the LLM (POST /vfx-suggest/api/generate)
/// or browse the built-in catalog, and send a preset to the particle editor
/// (POST /particle/api/effect, which relays it to the engine).
#include "explorer_view.h"

#include <set>

namespace ergo::studio::views {

class VfxSuggestView final : public ExplorerView {
public:
    /// `particle` may be null (no particle plugin loaded): sending is then hidden.
    VfxSuggestView(host::IPlugin& plugin, host::IPlugin* particle);

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void generate();
    void render_presets(ui::ViewBuilder& b, const char* slot_name, const JsonValue& list);
    void send_to_particle(const JsonValue& config, const std::string& name);

    std::unique_ptr<PluginApi> particle_;
    std::string                game_ = "generic";
    std::string                scene_;
    std::string                count_ = "3";
    std::set<std::string>      open_json_;   ///< "<slot>/<index>" of presets showing their JSON
};

} // namespace ergo::studio::views
