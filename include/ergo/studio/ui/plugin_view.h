#pragma once
/// IPluginView: a plugin's tab in the Studio window (the "view" capability of
/// the ISP plugin contract, design J5). GPU independent: a view only builds
/// ui_kit nodes through a ViewBuilder.
#include "ergo/studio/ui/view_builder.h"

#include <cstdint>

namespace ergo::studio::host {

class IPluginView {
public:
    virtual ~IPluginView() = default;
    /// Bumped whenever what the view shows changed; the app rebuilds then.
    virtual uint64_t view_revision() const = 0;
    /// Build the tab content. Called on the UI thread.
    virtual void build(ui::ViewBuilder& b) = 0;
};

} // namespace ergo::studio::host
