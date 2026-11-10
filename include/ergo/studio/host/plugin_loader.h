#pragma once
/// Instantiate every plugin once and validate it (port of instantiatePlugins()).
#include "ergo/studio/host/plugin.h"

#include <memory>
#include <string>
#include <vector>

namespace ergo::studio::host {

struct LoadedPlugin {
    std::unique_ptr<IPlugin> plugin;
    std::string              source;   ///< "built-in" or the pack entry path
    bool                     builtin = false;
};

/// An external entry that was skipped. Reported by /api/health.
struct RejectedPlugin {
    std::string id;        ///< the id it claimed, or "" when it never produced one
    std::string source;
    std::string reason;
};

struct LoadResult {
    std::vector<LoadedPlugin>   plugins;
    std::vector<RejectedPlugin> rejected;
};

/// Built-ins come first. A built-in that throws, returns nothing, has an
/// invalid / reserved id, an empty title or reuses an id throws
/// std::runtime_error (a bug in this repo). An external entry with the same
/// problem is logged, skipped and recorded in `rejected`. First claim wins.
LoadResult instantiate_plugins(const std::vector<PluginFactory>&   builtins,
                               const std::vector<ExternalFactory>& externals,
                               const LogFn&                        log);

} // namespace ergo::studio::host
