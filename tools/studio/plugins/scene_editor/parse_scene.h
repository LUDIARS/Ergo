#pragma once
/// Turn an engine's `scene` frame (ergo_scene Scene::to_json() text) into the
/// SceneInfo the editor shows. Pure and tolerant: a malformed document yields
/// nullopt; missing fields and vectors with non-number components fall back
/// to ergo_scene's defaults; a non-string scalar field is shown as JSON text.
#include "scene_info.h"

#include <optional>
#include <string>

namespace ergo::studio::plugins::scene_editor {

/// `id` is the relay key of the frame; it wins over the document's own id
/// because editor patches are routed by it.
std::optional<SceneInfo> parse_scene(const std::string& id, const std::string& json, const std::string& app);

} // namespace ergo::studio::plugins::scene_editor
