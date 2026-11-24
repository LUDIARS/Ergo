#pragma once
/// The Studio's UI as one Tela declaration (spec/plan/2026-11-23-studio-on-tela.md).
///
/// It only declares: the tab list on the left, the active tab in a scroll on
/// the right. Placement, input, focus, carets and scrolling belong to each
/// screen's tela::Runtime (the Tela window, every browser page), so a value
/// changed on one screen shows on all of them (the plugins own the values)
/// while focus and scroll stay per screen. The active tab is part of the
/// declaration and therefore shared.
///
/// With a launcher (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A / B)
/// the window (ScreenAccess::window) is the launcher only: its rows start and
/// stop the remote dev server. The browser pages get the launcher alone (with
/// "開く") until a project is open, then the project band over the tabs. A page
/// on the remote gate gets the launcher without add / remove (loopback only).
///
/// Single UI-thread owner: views and their callbacks run on that thread.
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/project/launcher.h"
#include "ergo/studio/ui/launcher_view.h"
#include "ergo/studio/tela/tela_emitter.h"
#include "ergo/studio/ui/studio_pages.h"

#include <tela/document.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ergo::studio::tela_shell {

/// Who looks at a screen. A page on the remote gate (Cloudflare Access /
/// tailnet) must not operate the tabs whose routes are loopback only (studio,
/// lesson). The window is this machine's launcher.
enum class ScreenAccess { local, published, window };

class StudioDeclaration {
public:
    /// `host` owns the plugins and outlives the declaration; so do `launcher`
    /// (optional: without one every screen gets the tabs from the start) and
    /// `devserver` (optional: the window's rows start / stop it).
    StudioDeclaration(host::PluginHost& host, ui::ViewLookup views, ui::Theme theme = {},
                      project::Launcher* launcher = nullptr, project::IDevServerControl* devserver = nullptr);

    /// Rebuild when an action / edit happened, the active view's revision moved,
    /// or (overview and generic tabs) health is due. True when it rebuilt.
    bool refresh(float dt);

    /// The declaration for a screen with that access.
    const ::tela::Document& document(ScreenAccess access) const;
    const ::tela::Theme&    theme() const { return tela_theme_; }
    /// Bumped on every rebuild (screens compare it with what they hold).
    uint64_t                revision() const { return revision_; }

    /// Tabs: 0 = overview, then one per plugin in load order.
    std::size_t tab_count() const { return 1 + host_.plugins().size(); }
    std::size_t active_tab() const { return active_; }
    void        select_tab(std::size_t i);
    /// True while the browser pages get the launcher instead of the tabs (the window always does).
    bool        showing_launcher() const;
    /// Id of the active tab's plugin ("" = overview).
    std::string active_plugin() const;
    void        set_health_interval(std::chrono::milliseconds ms) { health_interval_ = ms; }
    /// Something a callback changed: rebuild on the next refresh().
    void        touch() { dirty_ = true; }

    /// Plugins whose mutating routes are loopback only (mutation_guard.h).
    static bool loopback_only(const std::string& plugin_id);
    /// Id of the tab list and of a tab's scroll ("content/<plugin id | overview>").
    static const char* tabs_id() { return "tabs"; }
    /// Id of the launcher's scroll and of the open project's band.
    static const char* launcher_id() { return "launcher"; }
    static const char* project_id() { return "project"; }
    std::string        content_id() const;

private:
    void     rebuild();
    void     rebuild_launcher();
    ui::ViewNode build_launcher_body(const ui::LauncherScreen& screen);
    uint64_t launcher_revision() const;
    void     declare(::tela::Document& doc, const ui::ViewNode& tabs, const ui::ViewNode& body, bool locked);
    void     declare_band(::tela::Document& doc, const ui::ViewNode& band);
    void     declare_launcher(::tela::Document& doc, const ui::ViewNode& body);
    uint64_t content_revision() const;

    host::PluginHost&         host_;
    ui::ViewLookup            views_;
    ui::Theme                 theme_;
    project::Launcher*        launcher_ = nullptr;
    project::IDevServerControl* devserver_ = nullptr;
    ui::LauncherInput         launcher_input_;
    uint64_t                  built_launcher_revision_ = ~0ull;
    ::tela::Theme             tela_theme_;
    ApproximateMeasure        measure_;
    ::tela::Document          full_, published_, window_;
    uint64_t                  revision_ = 0;
    std::size_t               active_   = 0;
    bool                      dirty_    = true;
    uint64_t                  built_revision_ = ~0ull;
    std::chrono::milliseconds health_interval_{1000};
    float                     since_health_ = 0;
};

} // namespace ergo::studio::tela_shell
