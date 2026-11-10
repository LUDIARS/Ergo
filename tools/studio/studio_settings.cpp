#include "studio_settings.h"

#include <cstdlib>
#include <cstring>

namespace ergo::studio::app {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

/// UTF-8 text of a path in C++17 (std::string) and C++20 (std::u8string) alike.
std::string utf8(const std::filesystem::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

void take(const EnvFn& env, const char* key, std::string& out) {
    const std::string v = trim(env(key));
    if (!v.empty()) out = v;
}

} // namespace

StudioSettings read_settings(const EnvFn& env, const std::filesystem::path& exe_dir, int argc, char** argv) {
    StudioSettings s;
    take(env, "ERGO_HOST", s.bind_host);
    if (const std::string p = trim(env("PORT")); !p.empty()) {
        char* e = nullptr;
        const long n = std::strtol(p.c_str(), &e, 10);
        if (e != p.c_str() + p.size() || n < 1 || n > 65535) s.error = "PORT must be 1..65535 (got \"" + p + "\")";
        else s.port = static_cast<uint16_t>(n);
    }
#ifdef _WIN32
    take(env, "USERPROFILE", s.user_home);
#else
    take(env, "HOME", s.user_home);
#endif
    take(env, "ERGO_PLUGIN_DIR", s.plugin_dirs);
    take(env, "VISUS_PROJECT_ROOTS", s.visus_roots);
    take(env, "ERGO_UI_LAYOUT_ROOT", s.ui_layout_root);
    take(env, "ERGO_PICTOR_PROFILE_DIR", s.pictor_profile_dir);
    take(env, "ERGO_VFX_BACKEND", s.vfx_backend);
    take(env, "ERGO_VFX_MODEL", s.vfx_model);
    take(env, "CLAUDE_BIN", s.claude_bin);
    take(env, "ERGO_STUDIO_FONT", s.font_path);
    if (s.font_path.empty()) {
        // One documented default (not a search list): the system UI font for Japanese.
        const std::string windir = trim(env("WINDIR"));
        if (!windir.empty()) s.font_path = utf8(std::filesystem::u8path(windir) / "Fonts" / "meiryo.ttc");
    }
    s.shader_dir = utf8(exe_dir / "shaders");
    s.sdk_dir    = utf8(exe_dir / "sdk");
    s.studio     = plugins::studio::load_studio_config(env, s.user_home);
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0) s.headless = true;
    }
    return s;
}

} // namespace ergo::studio::app
