#pragma once
/// "profile" plugin: keeps the last ergo_profile Chrome trace published over
/// /profile/ws. The timeline view reads it through LatestBlobPlugin::current().
#include "ergo/studio/host/plugin.h"

#include <memory>

namespace ergo::studio::plugins::profile {

std::unique_ptr<host::IPlugin> make_profile_plugin(const host::LogFn& log);

} // namespace ergo::studio::plugins::profile
