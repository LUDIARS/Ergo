#pragma once
/// Native tab of "services": the workspace's services with their state and a
/// start / stop / restart button each (a start under someone else's test asks
/// before going ahead), and the recent log of the one selected.
#include "api_view.h"

#include <string>

namespace ergo::studio::views {

class ServicesView final : public ApiView {
public:
    explicit ServicesView(host::IPlugin& plugin) : ApiView(plugin) {}

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void control(const std::string& code, const std::string& action, bool force);
    void show_logs(const std::string& code);

    std::string selected_;   ///< service whose log is shown
    std::string contested_;  ///< a start refused because someone else is testing it
};

} // namespace ergo::studio::views
