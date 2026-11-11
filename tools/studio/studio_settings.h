#pragma once
/// Everything ergo_studio.exe reads from its environment, in one place. The
/// built-in plugins get their settings from here (they never read the
/// environment themselves), so a missing setting is visible in one list.
#include "plugins/studio/studio_config.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace ergo::studio::app {

using EnvFn = std::function<std::string(const std::string&)>;

/// Engine clients (ergo_bind, ergo_particle_link, ...) dial this port unless
/// told otherwise (design J4: the Node server's contract is kept).
constexpr uint16_t kDefaultPort = 5170;

struct StudioSettings {
    std::string bind_host = "127.0.0.1";   ///< ERGO_HOST
    uint16_t    port      = kDefaultPort;  ///< PORT
    std::string user_home;                 ///< USERPROFILE (Windows) / HOME
    std::string plugin_dirs;               ///< ERGO_PLUGIN_DIR
    std::string visus_roots;               ///< VISUS_PROJECT_ROOTS
    std::string ui_layout_root;            ///< ERGO_UI_LAYOUT_ROOT
    std::string pictor_profile_dir;        ///< ERGO_PICTOR_PROFILE_DIR
    std::string vfx_backend = "cli";       ///< ERGO_VFX_BACKEND
    std::string vfx_model   = "haiku";     ///< ERGO_VFX_MODEL
    std::string claude_bin  = "claude";    ///< CLAUDE_BIN
    std::string font_path;                 ///< ERGO_STUDIO_FONT, else <Windows>\Fonts\meiryo.ttc
    std::string shader_dir;                ///< <exe dir>/shaders
    std::string sdk_dir;                   ///< <exe dir>/sdk (plugin_abi.h for building packs)
    plugins::studio::StudioConfig studio;  ///< the studio plugin's settings (ERGO_STUDIO_HOME, LUDIARS_ROOT, ...)
    bool        headless  = false;         ///< --headless: server only, no window
    bool        show_help = false;         ///< --help / -h: print studio_usage() and start nothing
    std::string error;                     ///< an argument or setting that cannot be used (e.g. --foo, PORT=abc)
};

/// `exe_dir` is the directory of ergo_studio.exe. The command line accepts only
/// --headless and --help / -h; anything else sets `error` (arguments are checked
/// before the environment, so a typo is reported even when PORT is also bad).
StudioSettings read_settings(const EnvFn& env, const std::filesystem::path& exe_dir, int argc, char** argv);

/// The --help text: every option read_settings accepts and every environment
/// variable it (and the plugin configs it fills) reads. Keep in step with read_settings.
std::string studio_usage();

} // namespace ergo::studio::app
