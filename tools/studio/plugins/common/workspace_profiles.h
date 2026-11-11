#pragma once
/// Workspace profiles: what a game's workspace offers the game-ops tabs (the
/// services, tuning and git plugins). A profile is a JSON file:
///   {
///     "name":     "amicii",
///     "root":     "${LUDIARS_ROOT}/Amicii",          (optional; default: the file's folder)
///     "services": ["amicii-server", "amicii-server-tune"],   (Excubitor service codes)
///     "tuning":   {"amicii-server": "games/september/balance/versions"},  (ergo_bind app -> folder)
///     "git":      ["."]                               (repositories)
///   }
/// Paths are relative to `root`; "${NAME}" expands an environment variable (a
/// missing one makes the profile invalid, not silently empty). Profiles are
/// read from <studio home>/profiles/*.json and, for plugin packs, from
/// <each ERGO_PLUGIN_DIR folder>/*.studio.json (a pack ships its profile beside
/// its library).
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins {

struct WorkspaceProfile {
    std::string                                  name;
    std::filesystem::path                        source;    ///< the file it came from
    std::filesystem::path                        root;
    std::vector<std::string>                     services;
    std::map<std::string, std::filesystem::path> tuning;    ///< app -> versions folder (absolute)
    std::vector<std::filesystem::path>           git;       ///< repositories (absolute)
};

struct ProfileSet {
    std::vector<WorkspaceProfile> profiles;
    std::vector<std::string>      errors;   ///< "<file>: why" for each unusable file
};

using ProfileEnvFn = std::function<std::optional<std::string>(const std::string&)>;

/// Reads one profile's text (pure; `file` names where it came from).
std::optional<WorkspaceProfile> parse_workspace_profile(const std::string& text, const std::filesystem::path& file,
                                                        const ProfileEnvFn& env, std::string& err);

/// Every profile under `home`/profiles and the plugin folders (`plugin_dirs`
/// separated by ';'), in a stable order (folder, then file name).
ProfileSet load_workspace_profiles(const std::filesystem::path& home, const std::string& plugin_dirs,
                                   const ProfileEnvFn& env);

/// The process environment, for ProfileEnvFn.
std::optional<std::string> process_env(const std::string& name);

} // namespace ergo::studio::plugins
