#pragma once
/// The Studio's own pages, declared once for every backend: the tab list,
/// the overview (every plugin's health) and the generic tab of a plugin
/// without a view. StudioApp (ui_kit) and the Tela declaration both use them.
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/ui/plugin_view.h"
#include "ergo/studio/ui/view_builder.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace ergo::studio::ui {

/// Views for plugins that do not draw their own (IPlugin::view() == nullptr):
/// the exe attaches native tabs to built-in and external plugins by id.
using ViewLookup = std::function<host::IPluginView*(host::IPlugin&)>;

/// Title of the overview tab (tab 0).
const std::string& overview_title();

/// One line per tab (0 = overview, then the plugins in load order); a click selects it.
void build_tab_list(ViewBuilder& bar, const host::PluginHost& host, std::size_t active,
                    const std::function<void(std::size_t)>& select);
/// Every plugin's health and the external packs that were rejected.
void build_overview(ViewBuilder& b, const host::PluginHost& host);
/// Title, description, health fields and endpoints of a plugin without a view.
void build_generic(ViewBuilder& b, host::IPlugin& p);

/// The view of `p` (its own, else the lookup's), or null.
host::IPluginView* view_of(host::IPlugin& p, const ViewLookup& views);

} // namespace ergo::studio::ui
