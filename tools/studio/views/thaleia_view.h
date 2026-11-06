#pragma once
/// Native tab of "musa_thaleia": the spec document tree (folders open and
/// close), full-text search, and the selected document drawn from its block
/// JSON (GET /musa_thaleia/api/{tree,search,render}).
#include "explorer_view.h"

#include <set>

namespace ergo::studio::views {

class ThaleiaView final : public ExplorerView {
public:
    explicit ThaleiaView(host::IPlugin& plugin);

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void render_tree(ui::ViewBuilder& b, const JsonValue& node, int depth, int& budget);
    void render_document(ui::ViewBuilder& b);
    void open(const std::string& path);

    std::set<std::string> expanded_;
    std::string           selected_;
    std::string           query_;
};

/// Plain text of an inline array ([{kind, text}...]).
std::string inline_text(const JsonValue* inlines);

} // namespace ergo::studio::views
