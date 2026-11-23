#pragma once
/// Native tab of "assets" (spec/feature/tool/web.md の「アセット」を Studio のタブに):
/// the asset folders of the workspace profiles, browsed one folder at a time
/// (kind, size, modified time), and a text file's contents. Images are listed
/// with their size; drawing them needs an image decoder the Studio does not have yet.
#include "api_view.h"

#include <string>

namespace ergo::studio::views {

class AssetsView final : public ApiView {
public:
    explicit AssetsView(host::IPlugin& plugin) : ApiView(plugin) {}

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void open_root(int index);
    void open_dir(const std::string& dir);
    void open_file(const std::string& path, bool text);
    void render_listing(ui::ViewBuilder& b);
    void render_preview(ui::ViewBuilder& b);

    int         root_ = -1;    ///< selected folder (-1 = none)
    std::string dir_;          ///< folder shown, relative to the root ("" = the root)
    std::string file_;         ///< file previewed ("" = none)
    bool        file_text_ = false;
};

/// "12.3 KB" style size.
std::string human_size(double bytes);
/// The parent of a relative "a/b/c" path ("a/b"; "" for a top-level entry).
std::string parent_dir(const std::string& rel);

} // namespace ergo::studio::views
