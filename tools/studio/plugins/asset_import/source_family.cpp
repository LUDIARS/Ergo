#include "source_family.h"

#include <lorica/input_budget.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <utility>

namespace ergo::studio::plugins::asset_import {
namespace fs = std::filesystem;

SourceFormat source_format(const fs::path& file) {
    auto extension = file.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (extension == ".glb") return SourceFormat::Glb;
    if (extension == ".gltf") return SourceFormat::Gltf;
    if (extension == ".fbx") return SourceFormat::Fbx;
    if (extension == ".obj") return SourceFormat::Obj;
    if (extension == ".blend") return SourceFormat::Blend;
    return SourceFormat::Unknown;
}

fs::path source_family_key(const fs::path& file) {
    return file.parent_path() / file.stem();
}

SourceScan scan_source_families(const fs::path& root) {
    SourceScan scan;
    std::map<fs::path, std::vector<std::pair<SourceFormat, fs::path>>> by_stem;
    // Do not follow directory symlinks or treat linked files as import sources.
    lorica::input_budget budget({0, 10000, 10000, 32});
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
        if (!budget.consume(0, 1, 1)) {
            scan.entry_limit_reached = true;
            // A partial scan can miss a higher-priority model or drop an alternative.
            return scan;
        }
        if (!budget.permits_depth(static_cast<std::size_t>(it.depth()) + 1)) {
            it.disable_recursion_pending();
            continue;
        }
        if (!fs::is_regular_file(it->symlink_status())) continue;
        const auto format = source_format(it->path());
        if (format != SourceFormat::Unknown) by_stem[source_family_key(it->path())].emplace_back(format, it->path());
    }
    for (auto& entry : by_stem) {
        auto& files = entry.second;
        std::sort(files.begin(), files.end());
        if (files.front().first == SourceFormat::Blend) continue;
        SourceFamily family{std::move(files.front().second), {}};
        for (auto it = files.begin() + 1; it != files.end(); ++it) {
            family.alternatives.push_back(std::move(it->second));
        }
        scan.families.push_back(std::move(family));
    }
    // Preserve Visus name allocation order used for previously imported files.
    std::sort(scan.families.begin(), scan.families.end(),
              [](const auto& a, const auto& b) { return a.model < b.model; });
    return scan;
}

} // namespace ergo::studio::plugins::asset_import
