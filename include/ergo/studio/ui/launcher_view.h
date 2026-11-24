#pragma once
/// The launcher's screen and the band above an open project, declared with a
/// ViewBuilder (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A / B).
///
/// The window (StudioApp, the Tela window) is the launcher only: each row starts
/// or stops the remote dev server, and the screen shows where it answers. The
/// project's tabs are on the web side (/shell/): a browser page shows them
/// once a project is open, and before that the launcher with "開く" (a server
/// started without a project, ergo_studio --headless).
#include "ergo/studio/project/dev_server_control.h"
#include "ergo/studio/project/launcher.h"
#include "ergo/studio/ui/view_builder.h"

#include <functional>
#include <string>

namespace ergo::studio::ui {

/// What the launcher keeps between builds.
struct LauncherInput {
    std::string add_path;   ///< the path being typed for "追加"
    std::string error;      ///< the last failed operation ("" = none)
    std::string selected_source;
    bool reduced_motion{};
};

/// Who looks at the launcher.
struct LauncherScreen {
    /// A screen on this machine: only it shows remove and add (loopback-only operations).
    bool local = true;
    /// The window: rows start / stop the remote dev server. nullptr = a browser
    /// page: rows open the project in the server the page is on.
    project::IDevServerControl* devserver = nullptr;
};

/// The project list: one panel per row, then "add by path" (local screens).
/// `changed` runs after every operation (the caller rebuilds).
void build_launcher(ViewBuilder& b, project::Launcher& launcher, LauncherInput& input, const LauncherScreen& screen,
                    const std::function<void()>& changed);

/// The band over the tabs: the project's name and root, and back to the launcher.
void build_project_bar(ViewBuilder& b, const project::ProjectRef& project, ClickFn back);

} // namespace ergo::studio::ui
