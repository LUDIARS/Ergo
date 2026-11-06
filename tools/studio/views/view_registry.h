#pragma once
/// Native tabs by plugin id. A view is made the first time its tab is shown
/// (each owns a worker thread), dedicated ones for the plugins that have
/// them and the endpoint explorer for every other plugin (external packs too).
#include "ergo/studio/ui/studio_app.h"

#include <map>
#include <memory>

namespace ergo::studio::views {

/// The dedicated view for `plugin`'s id, or the explorer.
std::unique_ptr<host::IPluginView> make_view(host::IPlugin& plugin);

class ViewRegistry {
public:
    /// The lookup StudioApp calls on the UI thread. The registry must outlive the app.
    ui::ViewLookup lookup();

private:
    std::map<const host::IPlugin*, std::unique_ptr<host::IPluginView>> views_;
};

} // namespace ergo::studio::views
