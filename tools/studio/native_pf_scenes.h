#pragma once
#include "plugins/scene_editor/scene_editor_plugin.h"

#include <tela/document.hpp>

#include <future>

namespace ergo::studio::app {
// All UI state is confined to the window thread. Requests carry the project
// generation so a late response cannot populate a newly opened workspace.
class NativePfScenes {
public:
    void reset();
    bool update(plugins::scene_editor::SceneEditorPlugin& editor);
    void panel(::tela::Document& doc, ::tela::Layout layout);
    std::optional<std::string> runtime(
        const std::vector<plugins::scene_editor::SceneInfo>& scenes) const;

private:
    plugins::JsonValue catalog_, detail_;
    std::future<net::HttpResponse> request_;
    std::chrono::steady_clock::time_point next_{};
    uint64_t generation_ = 0, requested_generation_ = 0;
    bool expanded_ = false, dirty_ = true, needs_catalog_ = true, needs_detail_ = false;
    std::string layout_, spec_, note_, requested_spec_;
};
} // namespace ergo::studio::app
