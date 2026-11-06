#pragma once
/// Native tabs by plugin id. A view is made the first time its tab is shown
/// (each owns a worker thread), dedicated ones for the plugins that have
/// them and the endpoint explorer for every other plugin (external packs too).
#include "ergo/studio/ui/studio_app.h"

#include <functional>
#include <map>
#include <memory>

namespace ergo::studio::views {

/// Another loaded plugin by id (a view that hands data to a sibling), or null.
using PluginFinder = std::function<host::IPlugin*(const std::string& id)>;

/// The dedicated view for `plugin`'s id, or the explorer.
std::unique_ptr<host::IPluginView> make_view(host::IPlugin& plugin, const PluginFinder& find = {});

class ViewRegistry {
public:
    /// `host` (may be null) lets views reach sibling plugins; it must outlive the registry.
    explicit ViewRegistry(const host::PluginHost* host = nullptr) : host_(host) {}
    /// The lookup StudioApp calls on the UI thread. The registry must outlive the app.
    ui::ViewLookup lookup();

private:
    const host::PluginHost* host_;
    std::map<const host::IPlugin*, std::unique_ptr<host::IPluginView>> views_;
};

} // namespace ergo::studio::views
