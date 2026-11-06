#pragma once
/// Native tabs of the Ars-Musa plugins that edit settings and act on a
/// service: Terpsichore (command server: status / start / stop / config) and
/// Melpomene (GitHub issues: settings with a write-only token, list, detail
/// with comments, create, close, notifications).
#include "explorer_view.h"

namespace ergo::studio::views {

class TerpsichoreView final : public ExplorerView {
public:
    explicit TerpsichoreView(host::IPlugin& plugin);

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void load_config_text(const ApiResult& r);

    std::string config_text_;   ///< editable JSON of the config
};

class MelpomeneView final : public ExplorerView {
public:
    explicit MelpomeneView(host::IPlugin& plugin);

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    enum class Page { Issues, New, Notifications, Settings };
    void render_issues(ui::ViewBuilder& b);
    void render_issue(ui::ViewBuilder& b);
    void render_new(ui::ViewBuilder& b);
    void render_settings(ui::ViewBuilder& b);
    void open_issue(const std::string& number);
    void post_and_notice(const std::string& sub, const JsonValue& body, const std::string& ok_text,
                         std::function<void()> after = {});

    Page        page_ = Page::Issues;
    std::string filter_;
    std::string issue_;         ///< selected issue number
    std::string comment_;
    std::string new_title_, new_body_, new_labels_, new_scene_, new_object_;
    std::string owner_, repo_, token_;
    bool        settings_loaded_ = false;
};

} // namespace ergo::studio::views
