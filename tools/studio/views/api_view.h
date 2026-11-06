#pragma once
/// Base of the native tabs: named result slots filled by PluginApi calls on
/// the worker thread, read by build() on the UI thread. Every change bumps
/// view_revision(), which makes the app rebuild the tab.
#include "plugin_api.h"

#include "ergo/studio/ui/plugin_view.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace ergo::studio::views {

/// One slot: the latest answer to a named request.
struct Slot {
    bool      loading = false;
    bool      loaded  = false;
    ApiResult result;
};

class ApiView : public host::IPluginView {
public:
    explicit ApiView(host::IPlugin& plugin);
    ~ApiView() override;

    uint64_t view_revision() const override { return revision_.load(); }
    /// Locks the slots and calls render(). Views also refresh on their first build.
    void build(ui::ViewBuilder& b) final;

protected:
    /// Draw the tab. Slots may be read freely (the lock is held).
    virtual void render(ui::ViewBuilder& b) = 0;
    /// Requests to make when the tab is first shown (and on "refresh").
    virtual void on_first_show() {}

    /// Ask `sub_with_query` and store the answer in slot `name`. `then`
    /// runs on the worker after the slot is stored (with the lock held).
    void fetch(const std::string& name, const std::string& method, const std::string& sub_with_query,
               const std::string& body = {}, std::function<void(const ApiResult&)> then = {});
    void get(const std::string& name, const std::string& sub_with_query) { fetch(name, "GET", sub_with_query); }
    /// The slot (empty when never requested). Call from render() / then.
    const Slot& slot(const std::string& name) const;
    /// Mark changed from UI-thread state (a selection, a toggle).
    void touch() { ++revision_; }

    /// Common pieces: a "loading / error" line for a slot; true when the slot holds a good answer.
    bool slot_state(ui::ViewBuilder& b, const std::string& name) const;
    /// A message line at the top (the last action's result).
    void set_notice(std::string text, bool ok);
    void render_notice(ui::ViewBuilder& b) const;

    host::IPlugin& plugin_;

private:
    std::unique_ptr<PluginApi>   api_;
    mutable std::recursive_mutex mtx_;
    std::map<std::string, Slot>  slots_;
    std::atomic<uint64_t>        revision_{1};
    bool                         shown_ = false;
    std::string                  notice_;
    bool                         notice_ok_ = true;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

} // namespace ergo::studio::views
