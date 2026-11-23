#pragma once
/// The built-in plugins of ergo_studio.exe, in tab order.
#include "studio_settings.h"
#include "plugins/common/profile_source.h"

#include "ergo/studio/host/plugin.h"

#include <memory>
#include <vector>

namespace ergo::studio::app {

/// `profiles`: what the game-ops plugins (services, tuning, git, assets, play) look at
/// (the project open in the launcher).
std::vector<host::PluginFactory> builtin_plugins(const StudioSettings& s,
                                                 std::shared_ptr<const plugins::IProfileSource> profiles);

} // namespace ergo::studio::app
