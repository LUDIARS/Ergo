#pragma once

#include "import_assets.h"

namespace ergo::studio::plugins::asset_import {

struct CaptureRequest {
    std::string name;
    std::filesystem::path definition;
    std::filesystem::path source;
    std::string revision;
    bool capture = false;
    bool explicit_request = false;
};

using RequestPreview = std::function<bool(CaptureRequest)>;

std::string asset_revision(const std::string& definition, const std::filesystem::path& source);
bool store_capture(IAssetCatalog& catalog, const CaptureRequest& request, const std::string& png);
bool has_capture(const CatalogEntry& entry, const std::string& revision);
std::optional<std::string> read_capture(const CatalogEntry& entry, const std::string& revision);

} // namespace ergo::studio::plugins::asset_import
