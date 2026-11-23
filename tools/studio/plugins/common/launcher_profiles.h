#pragma once
/// What the launcher (ergo/studio/project/launcher.h) needs to know about
/// workspace profiles: read one file's name, root, style (the profile's
/// "style", else detected from the root) and domain, and find the profiles on disk
/// (<studio home>/profiles/*.json, <ERGO_PLUGIN_DIR>/*.studio.json).
#include "workspace_profiles.h"

#include "ergo/studio/project/launcher.h"

#include <string>

namespace ergo::studio::plugins {

/// Probe, discover and clock for the launcher.
project::LauncherDeps launcher_deps(const std::filesystem::path& home, const std::string& plugin_dirs,
                                    ProfileEnvFn env = &process_env);

/// Now as UTC ISO-8601 ("2026-11-23T01:02:03Z").
std::string utc_now_iso();

} // namespace ergo::studio::plugins
