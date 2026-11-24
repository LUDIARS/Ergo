#include "studio_settings.h"

#include "ergo/studio/ingress/entrance.h"
#include "ergo/studio/net/request_guard.h"

#include <cstdlib>

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

/// A port 1..65535, or nullopt.
std::optional<uint16_t> port_number(const std::string& text) {
    if (text.empty() || text.size() > 5) return std::nullopt;
    char*      e = nullptr;
    const long n = std::strtol(text.c_str(), &e, 10);
    if (e != text.c_str() + text.size() || n < 1 || n > 65535) return std::nullopt;
    return static_cast<uint16_t>(n);
}

bool is_option(const std::string& arg, const std::string& name) {
    return arg == name || arg.compare(0, name.size() + 1, name + "=") == 0;
}

/// The value of "--name value" or "--name=value", advancing `i` past it ("" when missing).
std::string option_value(int argc, char** argv, int& i, const std::string& arg, const std::string& name) {
    if (arg != name) return arg.substr(name.size() + 1);
    if (i + 1 >= argc || argv[i + 1] == nullptr) return {};
    return argv[++i];
}

/// The command line. Returns the error for the first argument that cannot be
/// used ("" when all are known).
std::string read_arguments(int argc, char** argv, StudioSettings& s) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i] ? argv[i] : "";
        if (a == "--headless") {
            s.headless = true;
        } else if (a == "--help" || a == "-h") {
            s.show_help = true;
        } else if (is_option(a, "--project")) {
            s.project = trim(option_value(argc, argv, i, a, "--project"));
            if (s.project.empty()) return "--project needs a profile path (ergo_studio --project <profile.json>)";
        } else if (is_option(a, "--remote-port")) {
            const std::string v = trim(option_value(argc, argv, i, a, "--remote-port"));
            s.remote_port       = port_number(v);
            if (!s.remote_port) return "--remote-port must be 1..65535 (got \"" + v + "\")";
        } else {
            const char* what = !a.empty() && a[0] == '-' ? "unknown option" : "unexpected argument";
            return std::string(what) + " \"" + a + "\" (ergo_studio --help lists the options)";
        }
    }
    return {};
}

} // namespace

StudioSettings read_settings(const EnvFn& env, const std::filesystem::path& exe_dir, int argc, char** argv) {
    StudioSettings s;
    s.error = read_arguments(argc, argv, s);
    take(env, "ERGO_HOST", s.bind_host);
    if (const std::string p = trim(env("PORT")); !p.empty()) {
        char* e = nullptr;
        const long n = std::strtol(p.c_str(), &e, 10);
        if (e != p.c_str() + p.size() || n < 1 || n > 65535) {
            if (s.error.empty()) s.error = "PORT must be 1..65535 (got \"" + p + "\")";
        } else {
            s.port = static_cast<uint16_t>(n);
        }
    }
#ifdef _WIN32
    take(env, "USERPROFILE", s.user_home);
#else
    take(env, "HOME", s.user_home);
#endif
    if (!s.remote_port) {
        if (const std::string p = trim(env("ERGO_STUDIO_REMOTE_PORT")); !p.empty()) {
            s.remote_port = port_number(p);
            if (!s.remote_port && s.error.empty()) s.error = "ERGO_STUDIO_REMOTE_PORT must be 1..65535 (got \"" + p + "\")";
        }
    }
    if (s.remote_port && *s.remote_port == s.port && s.error.empty()) {
        s.error = "the remote port must differ from PORT (" + std::to_string(s.port) + ")";
    }
    s.public_hosts  = net::parse_public_hosts(env("ERGO_STUDIO_PUBLIC_HOSTS"));
    s.tailnet_hosts = ingress::parse_tailnet_hosts(env("ERGO_STUDIO_TAILNET_HOSTS"));
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
    return s;
}

std::string studio_usage() {
    return "usage: ergo_studio [--headless] [--project <profile.json>] [--remote-port <port>]\n"
           "       ergo_studio --help | -h\n"
           "\n"
           "Ergo Studio: the launcher window and the web server of the project it serves (the remote dev\n"
           "server: HTTP / WebSocket for the browser and the engine clients). The window is the launcher;\n"
           "its \"start the remote dev server\" button starts the web server. Stop it with Ctrl+C (SIGINT).\n"
           "\n"
           "options:\n"
           "  --headless             no window: the web server runs from the start\n"
           "                         (a build without the window is always headless)\n"
           "  --project <path>       serve this workspace profile from the start (with --headless: how a\n"
           "                         site runs it as an Excubitor service)\n"
           "  --remote-port <port>   turn the remote gate on at 127.0.0.1:<port> (default: off)\n"
           "  --help, -h             print this text and exit (nothing is started)\n"
           "\n"
           "environment (unset or empty = the default):\n"
           "  PORT                     the local gate (this machine, every operation), 1..65535\n"
           "                           (default " + std::to_string(kDefaultPort) + ")\n"
           "  ERGO_STUDIO_REMOTE_PORT  the remote gate, the target of cloudflared / tailscale serve\n"
           "                           (default off; --remote-port wins). Loopback-only operations are refused there\n"
           "  ERGO_HOST                address to bind (default 127.0.0.1)\n"
           "  ERGO_STUDIO_PUBLIC_HOSTS host names published through Cloudflare Access, comma list (remote gate;\n"
           "                           answered only with the Cf-Access-Jwt-Assertion header; default none)\n"
           "  ERGO_STUDIO_TAILNET_HOSTS *.ts.net names served through tailscale serve, comma list (remote gate;\n"
           "                           answered only with the Tailscale-User-Login header; default none)\n"
           "  ERGO_STUDIO_FFMPEG       ffmpeg for remote play capture (default: ffmpeg on PATH)\n"
           "  ERGO_STUDIO_STUN_URLS    STUN servers for remote play, comma list (default stun.cloudflare.com)\n"
           "  ERGO_STUDIO_TURN_KEY_ID, ERGO_STUDIO_TURN_KEY_API_TOKEN\n"
           "                           Cloudflare TURN key for remote play (both or neither)\n"
           "  ERGO_PLUGIN_DIR          external plugin pack directories (separated by "
#ifdef _WIN32
           "';'"
#else
           "':'"
#endif
           ")\n"
           "  ERGO_STUDIO_HOME         plugin ledger and fetched plugins (default <home>/.ergo-studio)\n"
           "  LUDIARS_ROOT             where the Excubitor catalog and the Revisor CLI are (studio plugin)\n"
           "  EXCUBITOR_URL            Excubitor URL (default: from the catalog)\n"
           "  ERGO_ACTIO_TOKEN         Actio bearer token (unset = Actio local mode)\n"
           "  ERGO_CURARE_URL          a remote Curare server (default: from the Excubitor topology)\n"
           "  ERGO_CURARE_TOKEN        Curare bearer token\n"
           "  ERGO_PLUGIN_OWNERS       GitHub owners plugins may be fetched from, comma list (default LUDIARS)\n"
           "  ERGO_STUDIO_TIMEOUT_MS   limit of one service call in ms (default 8000)\n"
           "  VISUS_PROJECT_ROOTS      project roots the visus plugin scans (';' separated)\n"
           "  ERGO_UI_LAYOUT_ROOT      file root of the ui_layout plugin (unset = not configured)\n"
           "  ERGO_PICTOR_PROFILE_DIR  Pictor *.profile.json directory (render_pipeline plugin)\n"
           "  ERGO_VFX_BACKEND         vfx_suggest / vfx_breed backend (default cli, the only one)\n"
           "  ERGO_VFX_MODEL           model name passed to the claude CLI (default haiku)\n"
           "  CLAUDE_BIN               claude CLI to run (default claude)\n"
           "  ERGO_STUDIO_FONT         window font (default <WINDIR>\\Fonts\\meiryo.ttc when WINDIR is set)\n"
           "  WINDIR                   Windows directory, for the default font\n"
#ifdef _WIN32
           "  USERPROFILE              home directory (melpomene / terpsichore / thaleia, studio home)\n"
#else
           "  HOME                     home directory (melpomene / terpsichore / thaleia, studio home)\n"
#endif
           "\n"
           "At start-up the log prints the server address and each plugin with its path; plugins\n"
           "that clients reach directly add one line saying how (e.g. the variable WebSocket).\n";
}

} // namespace ergo::studio::app
