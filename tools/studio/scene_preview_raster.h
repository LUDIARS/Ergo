#pragma once
#include "plugins/scene_editor/scene_info.h"

#include <tela/drawing.hpp>

#include <memory>

namespace ergo::studio::app {
// Stores the diffuse term of the nearest surface. Lighting changes need no geometry pass.
struct ScenePreviewRaster {
    int width{}, height{};
    std::vector<float> diffuse;
    std::string notice;
    std::shared_ptr<const ::tela::Image> image(float intensity) const;
};
ScenePreviewRaster raster_scene(const plugins::scene_editor::SceneInfo&, int width, int height);
} // namespace ergo::studio::app
