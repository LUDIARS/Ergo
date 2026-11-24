#pragma once

#include "asset_catalog.h"
#include "model_analysis.h"

#include <functional>

namespace ergo::studio::plugins::asset_import {

using Analyze = std::function<ModelAnalysis(const std::string&)>;

struct ImportIssue {
    std::string source;
    std::string reason;
};

std::vector<ImportIssue> import_assets(const std::filesystem::path& inbox, IAssetCatalog& catalog,
                                      const Analyze& analyze = analyze_fbx);
inline constexpr std::size_t kMaxModelBytes = 64 * 1024 * 1024;
inline constexpr std::size_t kMaxVisusBytes = 2 * 1024 * 1024;
inline constexpr std::size_t kMaxThumbnailBytes = 1024 * 1024;
inline constexpr char kCatalogDirectory[] = ".ergo-visus";
inline constexpr char kImportRecord[] = "import.json";
inline constexpr char kThumbnailFile[] = "thumbnail.png";

} // namespace ergo::studio::plugins::asset_import
