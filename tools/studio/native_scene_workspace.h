#pragma once
#include "ergo/studio/project/open_project.h"
#include "ergo/studio/tela/studio_declaration.h"
#include "plugins/scene_editor/scene_editor_plugin.h"
#include "scene_preview_raster.h"
#include "native_pf_scenes.h"

#include <tela/runtime.hpp>

#include <future>

namespace ergo::studio::app {
class NativeSceneWorkspace {
public:
    NativeSceneWorkspace(host::PluginHost& plugins, project::OpenProject& project);
    std::string game_window() const;
    void preview_status(const std::string& message);
    bool update(::tela::Runtime& runtime, const tela_shell::StudioDeclaration& declaration);

private:
    struct Target {
        std::string name, app, scene, mode, state, window;
    };
    void load_targets();
    void run_target(const std::string& name, const std::string& action);
    void declare_play(::tela::Document&, const plugins::scene_editor::SceneInfo&, float width);
    host::IPluginHttp* play_ = nullptr;
    std::vector<Target> targets_;
    std::string variant_, active_target_, play_note_;
    bool variants_open_ = false;
    void declare_workspace(::tela::Document&, float width, float height);
    plugins::scene_editor::SceneEditorPlugin* editor_ = nullptr;
    project::OpenProject& project_;
    uint64_t project_version_ = ~0ull, scene_version_ = ~0ull, launcher_version_ = ~0ull;
    std::vector<plugins::scene_editor::SceneInfo> scenes_;
    std::string selected_, project_source_;
    std::future<ScenePreviewRaster> pending_;
    std::string pending_project_, pending_scene_;
    NativePfScenes pf_;
    ScenePreviewRaster raster_;
    std::shared_ptr<const ::tela::Image> image_;
    int width_ = 0, height_ = 0;
    bool open_ = false, dirty_ = true, geometry_dirty_ = true;
    float intensity_ = 1;
};
} // namespace ergo::studio::app
