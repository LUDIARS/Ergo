#pragma once

#include <filesystem>
#include <vector>

namespace ergo::studio::plugins::asset_import {

// Order used when choosing a model from a source family.
enum class SourceFormat { Glb, Gltf, Fbx, Obj, Blend, Unknown };

SourceFormat source_format(const std::filesystem::path& file);
std::filesystem::path source_family_key(const std::filesystem::path& file);

struct SourceFamily {
    std::filesystem::path model;
    std::vector<std::filesystem::path> alternatives;
};

struct SourceScan {
    std::vector<SourceFamily> families;
    bool entry_limit_reached = false;
};

SourceScan scan_source_families(const std::filesystem::path& root);

} // namespace ergo::studio::plugins::asset_import
