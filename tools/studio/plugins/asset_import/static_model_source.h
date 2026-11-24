#pragma once

#include "pictor/data/model_importer.h"

#include <filesystem>
#include <string>

namespace ergo::studio::plugins::asset_import {

pictor::ModelImportResult load_static_model(const std::filesystem::path& source,
                                           const std::string& bytes);

} // namespace ergo::studio::plugins::asset_import
