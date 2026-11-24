#pragma once

#include "source_family.h"

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
ModelAnalysis analyze_model(const std::filesystem::path& source, const std::string& bytes);
const char* model_loader_issue(SourceFormat format);

} // namespace ergo::studio::plugins::asset_import
