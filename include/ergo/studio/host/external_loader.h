#pragma once
/// External plugin packs (port of tools/ergo core/external.ts).
///
/// ERGO_PLUGIN_DIR names pack directories, separated by the OS path list
/// separator (";" on Windows, ":" elsewhere). Every immediate subdirectory of
/// a pack holding ERGO_STUDIO_PLUGIN_FILE (see plugin_abi.h) is one plugin.
/// Subdirectories are visited in name order so "first id wins" does not
/// depend on the file system. A library that cannot be loaded or lacks the
/// entry point is logged and skipped; it never aborts startup. Calling the
/// factory and validating the plugin is instantiate_plugins()'s job.
#include "ergo/studio/host/plugin.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ergo::studio::host {

/// Split an ERGO_PLUGIN_DIR value; blanks are dropped.
std::vector<std::filesystem::path> split_plugin_dirs(const std::string& raw);

/// Plugin library files of one pack, in subdirectory name order.
std::vector<std::filesystem::path> find_pack_entries(const std::filesystem::path& pack_dir, const LogFn& log);

/// Load every pack named in `raw` (the caller reads ERGO_PLUGIN_DIR).
std::vector<ExternalFactory> load_external_factories(const std::string& raw, const LogFn& log);

} // namespace ergo::studio::host
