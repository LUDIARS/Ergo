#pragma once
/// The launcher's screen and the band above an open project, declared with a
/// ViewBuilder (spec/plan/2026-11-23-studio-launcher-and-devserver.md, A).
/// StudioApp (ui_kit) and the Tela declaration show build_launcher() alone
/// until a project is open, then the band over the tabs.
#include "ergo/studio/project/launcher.h"
#include "ergo/studio/ui/view_builder.h"

#include <functional>
#include <string>

namespace ergo::studio::ui {

/// What the launcher keeps between builds.
struct LauncherInput {
    std::string add_path;   ///< the path being typed for "追加"
    std::string error;      ///< the last failed operation ("" = none)
};

/// The project list: one panel per row (open, remote dev server — not yet
/// available —, remove), then "add by path". `local` = a screen on this
/// machine: only it shows remove and add (loopback-only operations).
/// `changed` runs after every operation (the caller rebuilds).
void build_launcher(ViewBuilder& b, project::Launcher& launcher, LauncherInput& input, bool local,
                    const std::function<void()>& changed);

/// The band over the tabs: the project's name and root, and back to the launcher.
void build_project_bar(ViewBuilder& b, const project::ProjectRef& project, ClickFn back);

} // namespace ergo::studio::ui
