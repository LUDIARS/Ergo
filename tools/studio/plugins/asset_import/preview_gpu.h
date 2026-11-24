#pragma once

#include "preview_mesh.h"
#include "pictor/surface/vulkan_context.h"

#include <filesystem>

namespace ergo::studio::plugins::asset_import {

std::string draw_preview(pictor::VulkanContext& vk, const std::filesystem::path& shaders,
                         const std::vector<PreviewVertex>& vertices, bool capture);

} // namespace ergo::studio::plugins::asset_import
