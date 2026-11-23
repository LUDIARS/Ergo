#pragma once
/// Native tab of the "log" plugin: connected apps and their lines, filtered by
/// level / text / frame range (GET /log/api/apps, /log/api/lines). A poller
/// re-reads the selected app every second while "follow" is on.
#include "explorer_view.h"

#include <atomic>
#include <thread>

namespace ergo::studio::views {

class LogView final : public ExplorerView {
public:
    explicit LogView(host::IPlugin& plugin);
    ~LogView() override;

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void reload_lines();
    std::string lines_query() const;

    std::string app_;
    std::string levels_;     ///< "ERR,WRN" (empty = all)
    std::string text_;
    std::string frame_min_, frame_max_;
    std::string limit_ = "300";
    std::atomic<bool> follow_{false};
    std::atomic<bool> stop_{false};
    std::thread       poller_;
    std::mutex        query_mtx_;   ///< the poller reads the filter strings
    std::string       query_;       ///< last built query (poller's copy)
};

} // namespace ergo::studio::views
