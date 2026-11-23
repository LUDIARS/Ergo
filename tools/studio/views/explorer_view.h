#pragma once
/// The tab every plugin gets when it has no dedicated view (built-in or an
/// external pack): health, its known read endpoints as one-click requests,
/// and a free request form (method / path / JSON body) with the answer drawn
/// as JSON.
#include "api_view.h"

#include <string>
#include <vector>

namespace ergo::studio::views {

/// Read endpoints worth a button, by plugin id ("/api/health" is always first).
std::vector<std::string> known_get_endpoints(const std::string& plugin_id);

class ExplorerView : public ApiView {
public:
    explicit ExplorerView(host::IPlugin& plugin);

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

    /// The request / response part, for dedicated views that keep it below their own UI.
    void render_explorer(ui::ViewBuilder& b);

private:
    void send();

    std::string method_ = "GET";
    std::string path_   = "/api/health";
    std::string body_;
    std::string last_label_;
};

} // namespace ergo::studio::views
