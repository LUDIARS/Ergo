#pragma once
/// The built-in plugins of ergo_studio.exe, in tab order.
#include "studio_settings.h"

#include "ergo/studio/host/plugin.h"

#include <vector>

namespace ergo::studio::app {

std::vector<host::PluginFactory> builtin_plugins(const StudioSettings& s);

} // namespace ergo::studio::app
