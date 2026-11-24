#pragma once

#include "pictor/core/types.h"
#include <filesystem>
#include <string>
#include <vector>

namespace ergo::studio::plugins::asset_import {

struct PreviewVertex {
    pictor::float3 position;
    pictor::float3 color;
};

std::vector<PreviewVertex> preview_mesh(const std::filesystem::path& source, const std::string& bytes);

} // namespace ergo::studio::plugins::asset_import
