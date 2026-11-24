// ergo_studio.exe — reads the environment, builds the plugins, runs the
// launcher window and the web server of the project it serves (the remote dev
// server: spec/plan/2026-11-23-studio-launcher-and-devserver.md, B). Nothing
// else lives here (design: "the entry point only assembles host and shell").
#include "builtin_plugins.h"
#include "studio_settings.h"
#include "plugins/common/launcher_profiles.h"
#include "plugins/common/profile_source.h"
#include "plugins/studio/plugin_ledger.h"
#ifdef ERGO_STUDIO_HAS_ASSET_PREVIEW
#  include "plugins/asset_import/preview_window.h"
#endif
#include "plugins/studio/plugin_registry.h"

#include "ergo/studio/host/external_loader.h"
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/net/server.h"
#include "ergo/studio/project/dev_server.h"
#include "ergo/studio/project/launcher.h"
#include "ergo/studio/project/launcher_routes.h"
#include "ergo/studio/web/web_server.h"
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
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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

std::filesystem::path utf8_path(const std::string& text) {
#ifdef __cpp_char8_t
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
#else
    return std::filesystem::u8path(text);
#endif
}

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

/// What a started web server serves: its gates, and each plugin with its path.
void log_serving(const ergo::studio::web::WebServer& server, const ergo::studio::host::PluginHost& plugins,
                 const ergo::studio::app::StudioSettings& settings, const char* page) {
    using namespace ergo::studio;
    const std::string addr = settings.bind_host + ":" + std::to_string(server.local_port());
    log_line("info", "[ergo] local gate: http://" + addr + page);
    if (const auto remote = server.remote_port()) {
        log_line("info", "[ergo] remote gate: " + settings.bind_host + ":" + std::to_string(*remote) +
                             " (the target of cloudflared / tailscale serve; loopback-only operations refused)");
        for (const auto& h : settings.public_hosts) log_line("info", "[ergo]   https://" + h + page + " (Cloudflare Access)");
        for (const auto& h : settings.tailnet_hosts) log_line("info", "[ergo]   https://" + h + page + " (tailnet)");
    }
    for (auto* p : plugins.plugins()) {
        log_line("info", "[ergo]   -> " + p->info().title + "  (/" + p->info().id + "/" + (p->socket() ? ", ws" : "") + ")");
        if (!p->info().usage_hint.empty()) {
            log_line("info", "[ergo]        " + host::expand_usage_hint(p->info().usage_hint, addr));
        }
    }
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
    const std::string pack_dirs = plugins::studio::registry_plugin_dirs(
        settings.plugin_dirs, utf8_path(settings.studio.home), settings.studio.request_timeout_ms);
    // The launcher (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A): nothing is open on start;
    // the game-ops plugins see only the project opened there.
    const std::filesystem::path home = utf8_path(settings.studio.home);
    project::OpenProject open_project;
    project::Launcher    launcher(project::ProjectCatalog(home.empty() ? home : home / "recent_projects.json"),
                                  open_project, plugins::launcher_deps(home, settings.plugin_dirs));
    if (!launcher.notice().empty()) log_line("warn", "[ergo] " + launcher.notice());
    const auto profiles = std::make_shared<plugins::OpenProjectProfiles>(open_project);
    plugins::asset_import::RequestPreview request_preview;
#ifdef ERGO_STUDIO_HAS_ASSET_PREVIEW
    plugins::asset_import::PreviewWindow preview(utf8_path(settings.shader_dir), &log_line);
    if (!settings.headless) {
        request_preview = [&preview](plugins::asset_import::CaptureRequest request) {
            return preview.request(std::move(request));
        };
    }
#endif

    host::LoadResult loaded;
    try {
        loaded = host::instantiate_plugins(app::builtin_plugins(settings, profiles, std::move(request_preview)),
                                           host::load_external_factories(pack_dirs, &log_line), &log_line);
    } catch (const std::exception& e) {
        log_line("error", e.what());   // a built-in plugin is broken: a bug in this build
        return 1;
    }
    host::PluginHost plugins(std::move(loaded), &log_line);
    views::ViewRegistry views(&plugins);   // native tabs; outlives every screen

    // The remote dev server: started from the launcher (or at once, see below). The server is made
    // by `serve`, assigned once the routes exist (the declaration below needs the dev server first).
#ifdef ERGO_STUDIO_HAS_TELA
    const char* page = "/shell/";
#else
    const char* page = "/";
#endif
    project::ServeFn          serve;
    project::DevServerOptions dev_options{settings.bind_host, page, settings.public_hosts};
    dev_options.published_names.insert(dev_options.published_names.end(), settings.tailnet_hosts.begin(),
                                       settings.tailnet_hosts.end());
    project::DevServer devserver(launcher, [&serve](std::string& e) { return serve(e); }, dev_options);

#ifdef ERGO_STUDIO_HAS_TELA
    // One Tela declaration for every screen: the window (the launcher) and the browser pages of /shell/
    // (spec/feature/tool/shell.md). The routes sit in front of the plugin host.
    tela_shell::StudioDeclaration declaration(plugins, views.lookup(), ui::Theme{}, &launcher, &devserver);
    tela_shell::BrowserScreens    pages(&log_line);
    tela_shell::ScreenHub         hub(declaration, pages);
    tela_shell::ShellRoutes       routes(plugins, plugins, pages);
    net::IHttpHandler&            http_handler = routes;
    net::IWsAcceptor&             ws_acceptor  = routes;
#else
    net::IHttpHandler& http_handler = plugins;
    net::IWsAcceptor&  ws_acceptor  = plugins;
#endif
    project::LauncherRoutes launcher_routes(launcher, http_handler);   // /api/launcher/*

    std::vector<std::string> plugin_ids;
    for (auto* p : plugins.plugins()) plugin_ids.push_back(p->info().id);
    web::WebServerConfig web_config;
    web_config.bind_host     = settings.bind_host;
    web_config.local_port    = settings.port;
    web_config.remote_port   = settings.remote_port;
    web_config.public_hosts  = settings.public_hosts;
    web_config.tailnet_hosts = settings.tailnet_hosts;
    serve = [&](std::string& e) -> std::unique_ptr<project::IServing> {
        auto server = std::make_unique<web::WebServer>(web_config, launcher_routes, ws_acceptor, plugin_ids, &log_line);
        if (!server->start(e)) return nullptr;
        log_serving(*server, plugins, settings, page);
        return server;
    };
    for (const auto& r : plugins.rejected()) log_line("error", "[ergo]   !! rejected " + r.source + ": " + r.reason);

#if defined(ERGO_STUDIO_HAS_TELA_WINDOW) || defined(ERGO_STUDIO_HAS_SHELL)
    const bool windowed = !settings.headless;
#else
    const bool windowed = false;   // a build without the window is always headless
#endif
    // --project serves that profile at once; without a window the server runs from the start (no project
    // open: the launcher's HTTP API opens one); with a window it waits for "リモート開発サーバ起動".
    std::string err;
    if (!settings.project.empty()) {
        if (!devserver.start(settings.project, err)) {
            log_line("error", "[ergo] cannot serve " + settings.project + ": " + err);
            return 1;
        }
    } else if (!windowed) {
        if (!devserver.serve_without_project(err)) {
            log_line("error", "[ergo] " + err);
            return 1;
        }
    } else {
        log_line("info", "[ergo] launcher: start the remote dev server of a project to serve it");
    }

    int code = 0;
#if defined(ERGO_STUDIO_HAS_TELA_WINDOW)
    if (!settings.headless) {
        tela_shell::TelaWindowConfig wc;
        wc.font_path  = settings.font_path;
        wc.shader_dir = settings.shader_dir;
#ifdef ERGO_STUDIO_HAS_ASSET_PREVIEW
        wc.on_frame = [&] {
            const auto project = open_project.current();
            preview.pump(project ? utf8_path(project->root) : std::filesystem::path{});
        };
        wc.before_close = [&] { preview.close(); };
#endif
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
        wc.devserver  = &devserver;
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
    devserver.stop();
    return code;
}
