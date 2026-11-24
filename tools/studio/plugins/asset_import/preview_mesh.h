#pragma once

#include "source_family.h"
#include "pictor/core/types.h"
#include <string>
#include <vector>

namespace ergo::studio::plugins::asset_import {

struct PreviewVertex {
    pictor::float3 position;
    pictor::float3 color;
};

std::vector<PreviewVertex> preview_mesh(SourceFormat format, const std::string& bytes);

} // namespace ergo::studio::plugins::asset_import
