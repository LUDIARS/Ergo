#pragma once
/// Native tab of "lookdev": the look settings a running game publishes
/// (ergo_bind variables in the "look/..." categories, held by the variable
/// plugin) as sliders, toggles and number fields grouped by category, edited
/// live while the game is played in its own window; presets saved and loaded
/// through /lookdev/api/presets; "撮影して投稿" asks the game for a still
/// (look.capture_request), waits for look.capture_path and posts the still,
/// its preset and what changed since the game connected (/lookdev/api/post).
#include "explorer_view.h"

#include <map>
#include <string>
#include <vector>

namespace ergo::studio::plugins::variable {
class VariablePlugin;
struct BoundVar;
}

namespace ergo::studio::views {

class LookdevView final : public ExplorerView {
public:
    /// `variable` may be null (no variable plugin loaded): the tab then says so.
    LookdevView(host::IPlugin& plugin, host::IPlugin* variable);

    uint64_t view_revision() const override;

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void render_actions(ui::ViewBuilder& b, const std::vector<plugins::variable::BoundVar>& vars);
    void render_group(ui::ViewBuilder& b, const std::vector<plugins::variable::BoundVar>& vars);
    void render_number(ui::ViewBuilder& b, const plugins::variable::BoundVar& v);
    void render_vector(ui::ViewBuilder& b, const plugins::variable::BoundVar& v);
    void set(const std::string& name, const JsonValue& value);
    void remember_baseline(const std::vector<plugins::variable::BoundVar>& vars);
    std::vector<std::string> changes(const std::vector<plugins::variable::BoundVar>& vars) const;
    void start_capture();
    void check_capture(const std::vector<plugins::variable::BoundVar>& vars);
    void save_preset(const std::vector<plugins::variable::BoundVar>& vars);
    void load_preset(const std::string& name);

    plugins::variable::VariablePlugin* variable_ = nullptr;
    std::string group_;                           ///< selected category ("Levels")
    std::map<std::string, std::string> edits_;    ///< number field text per variable
    std::map<std::string, std::string> baseline_; ///< value (as JSON) when the game connected
    std::string app_;                             ///< the game's ergo_bind app name
    std::string comment_;
    std::string preset_name_;
    bool        capturing_ = false;               ///< waiting for the game to write a still
    std::string previous_capture_;                ///< look.capture_path before the request
};

} // namespace ergo::studio::views
