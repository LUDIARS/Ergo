#pragma once
/// Workspace profiles: what a game's workspace offers the game-ops tabs (the
/// services, tuning and git plugins). A profile is a JSON file:
///   {
///     "name":     "amicii",
///     "root":     "${LUDIARS_ROOT}/Amicii",          (optional; default: the file's folder)
///     "style":    "unity",                            (optional; unity | cpp-ludiars | cpp-other | other,
///                                                      wins over detection: ergo/studio/project/project_style.h)
///     "domain":   "game",                             (optional; what the project is for: lesson, game, cglive,
///                                                      industrial or any other text; shown, not interpreted)
///     "services": ["amicii-server", "amicii-server-tune"],   (Excubitor service codes)
///     "tuning":   {"amicii-server": "games/september/balance/versions"},  (ergo_bind app -> folder)
///     "git":      ["."],                              (repositories)
///     "assets":   [{"name": "Models", "path": "games/september/models"}]  (asset folders, read only)
///     "import":   "assets/incoming",                (optional; incoming FBX folder)
///     "play":     [{"name": "game", "run": {...}, "window": "Game"}]  (play plugin; read there, play_target.h)
///   }
/// Paths are relative to `root`; "${NAME}" expands an environment variable (a
/// missing one makes the profile invalid, not silently empty). Profiles are
/// read from <studio home>/profiles/*.json and, for plugin packs, from
/// <each ERGO_PLUGIN_DIR folder>/*.studio.json (a pack ships its profile beside
/// its library).
#include "json_util.h"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins {

/// A folder of game assets the assets plugin lists (read only).
struct AssetRoot {
    std::string           name;   ///< label shown on the page
    std::filesystem::path path;   ///< absolute
};

struct WorkspaceProfile {
    std::string                                  name;
    std::filesystem::path                        source;    ///< the file it came from
    std::filesystem::path                        root;
    std::string                                  style;     ///< explicit "style" ("" = detect from root)
    std::string                                  domain;    ///< "domain" as written ("" = none)
    std::vector<std::string>                     services;
    std::map<std::string, std::filesystem::path> tuning;    ///< app -> versions folder (absolute)
    std::vector<std::filesystem::path>           git;       ///< repositories (absolute)
    std::vector<AssetRoot>                       assets;    ///< asset folders, in file order
    std::filesystem::path                        import_dir;
    JsonValue                                    tuning_templates;
    JsonValue                                    play;      ///< the raw "play" value (null when absent)
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
