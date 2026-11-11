#pragma once
/// Native tab of "git": the workspace repositories; for the one selected, its
/// branch and how far it is from the upstream, the changed files, the branches
/// (switch or create), a commit message box (stages everything and commits),
/// fetch / pull / push, and the latest commits.
#include "api_view.h"

#include <string>

namespace ergo::studio::views {

class GitView final : public ApiView {
public:
    explicit GitView(host::IPlugin& plugin) : ApiView(plugin) {}

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void select(int index);
    void refresh();
    void post(const std::string& what, const std::string& body, const std::string& done);

    int         repo_ = -1;
    std::string new_branch_;
    std::string message_;
};

} // namespace ergo::studio::views
