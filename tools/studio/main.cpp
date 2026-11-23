// ergo_studio.exe — reads the environment, builds the plugins, serves the
// engine-facing HTTP / WS contract and runs the window. Nothing else lives
// here (design: "the entry point only assembles host and shell").
#include "builtin_plugins.h"
#include "studio_settings.h"
#include "plugins/common/launcher_profiles.h"
#include "plugins/common/profile_source.h"
#include "plugins/studio/plugin_ledger.h"

#include "ergo/studio/host/external_loader.h"
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/net/server.h"
#include "ergo/studio/project/launcher.h"
#include "ergo/studio/project/launcher_routes.h"
#include "views/view_registry.h"
#ifdef ERGO_STUDIO_HAS_TELA
#  include "ergo/studio/tela/screen_hub.h"
#  include "ergo/studio/tela/shell_routes.h"
#endif
#ifdef ERGO_STUDIO_HAS_TELA_WINDOW
#  include "ergo/studio/tela/tela_window.h"
#elif defined(ERGO_STUDIO_HAS_SHELL)
#  include "ergo/studio/shell/studio_window.h"
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <crtdbg.h>
#endif

namespace {

std::atomic<bool> g_stop{false};
std::mutex        g_log_mtx;

void log_line(const std::string& level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_log_mtx);
    std::FILE* out = level == "info" ? stdout : stderr;
    const char* tag = level == "info" ? "[info]" : level == "warn" ? "[warn]" : "[err]";
    std::fprintf(out, "%s %s\n", tag, msg.c_str());
    std::fflush(out);
}

std::string env(const std::string& key) {
#ifdef _WIN32
    // Wide API: values (paths) may hold non-ASCII characters.
    const std::wstring wkey(key.begin(), key.end());
    const DWORD n = GetEnvironmentVariableW(wkey.c_str(), nullptr, 0);
    if (n == 0) return {};
    std::wstring w(n, L'\0');
    GetEnvironmentVariableW(wkey.c_str(), w.data(), n);
    w.resize(n - 1);
    const auto u = std::filesystem::path(w).u8string();   // std::u8string under C++20
    return std::string(u.begin(), u.end());
#else
    const char* v = std::getenv(key.c_str());
    return v ? v : "";
#endif
}

std::filesystem::path exe_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    return std::filesystem::path(std::wstring(buf, n)).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

} // namespace

int main(int argc, char** argv) {
    using namespace ergo::studio;
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#  if defined(_DEBUG)
    // A debug assertion must reach the log, not a modal box nobody sees
    // (the Studio is started by Excubitor without a console).
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#  endif
#endif
    std::signal(SIGINT, [](int) { g_stop = true; });

    const auto settings = app::read_settings(&env, exe_dir(), argc, argv);
    if (settings.show_help && settings.error.empty()) {
        std::fputs(app::studio_usage().c_str(), stdout);
        return 0;
    }
    if (!settings.error.empty()) {
        log_line("error", "[ergo] " + settings.error);
        return 1;
    }

    // ERGO_PLUGIN_DIR plus the packs enabled in the Studio plugin ledger.
    const std::string pack_dirs = settings.studio.home.empty()
        ? settings.plugin_dirs
        : plugins::studio::plugin_dirs_with_ledger(settings.plugin_dirs,
                                                   plugins::studio::read_ledger(std::filesystem::u8path(settings.studio.home)));
    // The launcher (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A): nothing is open on start;
    // the game-ops plugins see only the project opened there.
    const std::filesystem::path home = std::filesystem::u8path(settings.studio.home);
    project::OpenProject open_project;
    project::Launcher    launcher(project::ProjectCatalog(home.empty() ? home : home / "recent_projects.json"),
                                  open_project, plugins::launcher_deps(home, settings.plugin_dirs));
    if (!launcher.notice().empty()) log_line("warn", "[ergo] " + launcher.notice());
    const auto profiles = std::make_shared<plugins::OpenProjectProfiles>(open_project);

    host::LoadResult loaded;
    try {
        loaded = host::instantiate_plugins(app::builtin_plugins(settings, profiles),
                                           host::load_external_factories(pack_dirs, &log_line), &log_line);
    } catch (const std::exception& e) {
        log_line("error", e.what());   // a built-in plugin is broken: a bug in this build
        return 1;
    }
    host::PluginHost plugins(std::move(loaded), &log_line);
    views::ViewRegistry views(&plugins);   // native tabs; outlives every screen

#ifdef ERGO_STUDIO_HAS_TELA
    // One Tela declaration for every screen: the window and the browser pages of /shell/
    // (spec/feature/tool/shell.md). The routes sit in front of the plugin host.
    tela_shell::StudioDeclaration declaration(plugins, views.lookup(), ui::Theme{}, &launcher);
    tela_shell::BrowserScreens    pages(&log_line);
    tela_shell::ScreenHub         hub(declaration, pages);
    tela_shell::ShellRoutes       routes(plugins, plugins, pages, settings.bind_host);
    net::IHttpHandler&            http_handler = routes;
    net::IWsAcceptor&             ws_acceptor  = routes;
#else
    net::IHttpHandler& http_handler = plugins;
    net::IWsAcceptor&  ws_acceptor  = plugins;
#endif
    project::LauncherRoutes launcher_routes(launcher, http_handler, settings.bind_host);   // /api/launcher/*

    net::Server server({settings.bind_host, settings.port, settings.public_hosts}, launcher_routes, ws_acceptor, &log_line);
    for (const auto& h : settings.public_hosts) log_line("info", "[ergo] published as https://" + h + "/web/ (Cloudflare Access only)");
    std::string err;
    if (!server.start(err)) {
        log_line("error", "[ergo] " + err);
        return 1;
    }
    const std::string addr = settings.bind_host + ":" + std::to_string(server.port());
    log_line("info", "[ergo] http://" + addr + "/");
#ifdef ERGO_STUDIO_HAS_TELA
    log_line("info", "[ergo] browser shell: http://" + addr + "/shell/");
#endif
    for (auto* p : plugins.plugins()) {
        log_line("info", "[ergo]   -> " + p->info().title + "  (/" + p->info().id + "/" + (p->socket() ? ", ws" : "") + ")");
        if (!p->info().usage_hint.empty()) {
            log_line("info", "[ergo]        " + host::expand_usage_hint(p->info().usage_hint, addr));
        }
    }
    for (const auto& r : plugins.rejected()) log_line("error", "[ergo]   !! rejected " + r.source + ": " + r.reason);

    int code = 0;
#if defined(ERGO_STUDIO_HAS_TELA_WINDOW)
    if (!settings.headless) {
        tela_shell::TelaWindowConfig wc;
        wc.font_path  = settings.font_path;
        wc.shader_dir = settings.shader_dir;
        try {
            code = tela_shell::run_tela_window(hub, wc, g_stop, err);
        } catch (const std::exception& e) {
            err  = std::string("unexpected exception: ") + e.what();
            code = 9;
        }
        if (code != 0) log_line("error", "[ergo] window: " + err);
    } else
#elif defined(ERGO_STUDIO_HAS_SHELL)
    if (!settings.headless) {
        shell::WindowConfig wc;
        wc.font_path  = settings.font_path;
        wc.shader_dir = settings.shader_dir;
        wc.views      = views.lookup();
        wc.launcher   = &launcher;
#  ifdef ERGO_STUDIO_HAS_TELA
        wc.on_frame = [&hub](float dt) { hub.pump(dt); };   // browser pages share the window's UI thread
#  endif
        try {
            code = shell::run_studio_window(plugins, wc, g_stop, err);
        } catch (const std::exception& e) {
            err  = std::string("unexpected exception: ") + e.what();
            code = 9;
        }
        if (code != 0) log_line("error", "[ergo] window: " + err);
    } else
#endif
    {
#ifdef ERGO_STUDIO_HAS_TELA
        // No window: this thread is the UI thread of the browser pages.
        auto last = std::chrono::steady_clock::now();
        while (!g_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            const auto now = std::chrono::steady_clock::now();
            hub.pump(std::chrono::duration<float>(now - last).count());
            last = now;
        }
#else
        while (!g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
    }
    server.stop();
    return code;
}
