#pragma once
/// Native tab of the "studio" plugin: projects (Cc) with their source state
/// and the AI instruction form, tasks (Actio) with create / implement,
/// delegation runs, and the service connection map. All through
/// /studio/api/* (spec/feature/tool/studio.md).
#include "explorer_view.h"

namespace ergo::studio::views {

class StudioView final : public ExplorerView {
public:
    explicit StudioView(host::IPlugin& plugin);

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    enum class Page { Projects, Tasks, Runs, Connections, Api };

    void render_pages(ui::ViewBuilder& b);
    void render_projects(ui::ViewBuilder& b);
    void render_project_detail(ui::ViewBuilder& b, const JsonValue& project);
    void render_templates(ui::ViewBuilder& b);
    void render_tasks(ui::ViewBuilder& b);
    void render_runs(ui::ViewBuilder& b);
    void render_connections(ui::ViewBuilder& b);

    void select_project(const std::string& code);
    void instruct();
    void create_task();
    void implement_task(const std::string& task_id);
    /// POST and report the answer as the notice; `refresh` re-reads a slot after.
    void post(const std::string& sub, const JsonValue& body, const std::string& done_text, const std::string& refresh_slot,
              const std::string& refresh_path);

    Page        page_ = Page::Projects;
    std::string project_;     ///< selected project code
    std::string template_;    ///< selected delegation template (call_name)
    std::string task_id_;     ///< selected task
    std::string instruction_;
    std::string filter_;
    std::string new_title_, new_description_;
};

} // namespace ergo::studio::views
