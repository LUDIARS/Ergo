#pragma once

#include <array>
#include <string>
#include <vector>

#include "pictor/visus/visus.h"

namespace ergo::studio::plugins::asset_import {

struct ModelAnalysis {
    std::array<double, 3> minimum{};
    std::array<double, 3> maximum{};
    std::vector<pictor::VisusPart> parts;
    pictor::VisusValue::Array clips;
};

ModelAnalysis analyze_fbx(const std::string& bytes);

} // namespace ergo::studio::plugins::asset_import
