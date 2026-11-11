#pragma once
/// Native tab of "tuning": the values every connected game publishes through
/// ergo_bind (held by the variable plugin), one app at a time, grouped by
/// category ("balance/exhibits/telephone", "look/Night", ...), with a slider
/// and a number field each, edited live. Below, the app's versions: save the
/// current values with a name and a note, compare a version with the live
/// values, restore one.
#include "api_view.h"

#include <map>
#include <string>
#include <vector>

namespace ergo::studio::plugins::variable {
class VariablePlugin;
struct BoundVar;
}

namespace ergo::studio::views {

class TuningView final : public ApiView {
public:
    /// `variable` may be null (no variable plugin loaded): the tab then says so.
    TuningView(host::IPlugin& plugin, host::IPlugin* variable);

    uint64_t view_revision() const override;

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void render_values(ui::ViewBuilder& b, const std::vector<plugins::variable::BoundVar>& vars);
    void render_number(ui::ViewBuilder& b, const plugins::variable::BoundVar& v);
    void render_versions(ui::ViewBuilder& b, const std::vector<plugins::variable::BoundVar>& vars);
    void load_versions();
    void save_version(const std::vector<plugins::variable::BoundVar>& vars);
    void open_version(const std::string& id, bool restore);
    void set(const std::string& name, const common::jsonm::JsonValue& value);

    plugins::variable::VariablePlugin* variable_ = nullptr;
    std::string app_;                           ///< the app shown
    std::string group_;                         ///< the category shown
    std::map<std::string, std::string> edits_;  ///< number field text per variable
    std::string version_name_, version_note_;
    std::string compared_;                      ///< the version whose differences are shown
};

} // namespace ergo::studio::views
