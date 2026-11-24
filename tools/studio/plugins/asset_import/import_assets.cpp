#include "import_assets.h"

#include "../common/safe_path.h"
#include "ergo/studio/platform/sha256.h"
#include "pictor/visus/visus_serializer.h"

#include <lorica/input_budget.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {
namespace fs = std::filesystem;
namespace {

std::string path_from(const fs::path& directory, const fs::path& file) {
    const auto relative = file.lexically_relative(directory);
    return (relative.empty() ? file : relative).generic_u8string();
}

pictor::VisusValue point(const std::array<double, 3>& p) {
    return pictor::VisusValue::Array{p[0], p[1], p[2]};
}

std::string definition(const CatalogEntry& entry, const fs::path& source, const fs::path& inbox,
                       const std::string& hash, ModelAnalysis analysis) {
    pictor::VisusDesc desc;
    desc.name = entry.name;
    desc.kind = pictor::VisusKind::MODEL;
    desc.asset = path_from(entry.directory, source);
    desc.parts = std::move(analysis.parts);
    for (auto& part : desc.parts) {
        std::vector<std::pair<std::string, std::string>> textures;
        for (const auto& [key, value] : part.metadata) {
            if (key.compare(0, 8, "texture.") != 0) continue;
            const auto requested = fs::u8path(value.as_string());
            const auto relative = requested.is_absolute() ? requested.filename() : requested;
            auto path = (source.parent_path() / relative).lexically_normal();
            const auto safe = resolve_within(inbox, path_from(inbox, path));
            std::error_code ec;
            textures.emplace_back(key, safe && fs::is_regular_file(*safe, ec)
                ? path_from(entry.directory, *safe) : std::string{});
        }
        for (const auto& [key, path] : textures) {
            if (path.empty()) part.metadata.erase(key);
            else part.metadata.set(key, path);
        }
    }
    desc.metadata.set(pictor::visus_keys::kAnimationClips, std::move(analysis.clips));
    desc.metadata.set("import.source", source.generic_u8string());
    desc.metadata.set("import.sha256", hash);
    desc.metadata.set("import.bounds.min", point(analysis.minimum));
    desc.metadata.set("import.bounds.max", point(analysis.maximum));
    return pictor::to_visus_json(desc);
}

struct Generated {
    CatalogEntry entry;
    std::string bytes;
    std::string hash;
};

std::map<std::string, Generated> generated_entries(IAssetCatalog& catalog) {
    std::map<std::string, Generated> generated;
    for (const auto& entry : catalog.entries()) {
        auto bytes = read_bounded(entry.directory / kImportRecord, kMaxVisusBytes);
        pictor::VisusDesc desc;
        if (!bytes || !pictor::from_visus_json(*bytes, desc) || entry.name != desc.name) continue;
        const auto source = desc.metadata.get_string("import.source");
        const auto hash = desc.metadata.get_string("import.sha256");
        if (source && hash) generated.emplace(*source, Generated{entry, std::move(*bytes), *hash});
    }
    return generated;
}

CatalogEntry allocate(const fs::path& source, IAssetCatalog& catalog) {
    const auto stem = source.stem().u8string();
    for (std::size_t index = 1; index <= 10000; ++index) {
        auto entry = catalog.find(index == 1 ? stem : stem + "_" + std::to_string(index));
        if (!fs::exists(entry.directory)) return entry;
    }
    throw std::runtime_error("Visus name allocation limit reached");
}

void import_one(const fs::path& file, const fs::path& inbox, IAssetCatalog& catalog,
                const Analyze& analyze, const std::map<std::string, Generated>& generated) {
    auto bytes = read_bounded(file, kMaxModelBytes);
    if (!bytes) throw std::runtime_error("FBX could not be read within the file size limit");
    const auto hash = platform::sha256_hex(*bytes);
    const auto previous = generated.find(file.generic_u8string());
    if (previous != generated.end()) {
        const auto& entry = previous->second.entry;
        auto current = read_bounded(entry.visus_path(), kMaxVisusBytes);
        if (current && *current != previous->second.bytes) return;
        if (!current && fs::exists(entry.visus_path())) {
            throw std::runtime_error("existing Visus cannot be read");
        }
        if (previous->second.hash == hash) {
            if (!current) catalog.put(entry, entry.visus_filename(), previous->second.bytes);
            return;
        }
    }
    auto analysis = analyze(*bytes);
    const auto entry = previous == generated.end() ? allocate(file, catalog) : previous->second.entry;
    auto json = definition(entry, file, inbox, hash, std::move(analysis));
    if (json.size() > kMaxVisusBytes) throw std::runtime_error("generated Visus exceeds file size limit");
    if (previous != generated.end()) {
        // Parsing can take time. Recheck the edited document immediately before publishing.
        const auto current = read_bounded(entry.visus_path(), kMaxVisusBytes);
        if (current ? *current != previous->second.bytes : fs::exists(entry.visus_path())) return;
    }
    catalog.put(entry, entry.visus_filename(), json);
    catalog.put(entry, kImportRecord, json);
}

} // namespace

std::vector<ImportIssue> import_assets(const fs::path& inbox, IAssetCatalog& catalog, const Analyze& analyze) {
    std::vector<ImportIssue> issues;
    std::vector<fs::path> files;
    // No symlink following. Bound directory traversal separately from per-file decoding.
    lorica::input_budget scan({0, 10000, 10000, 32});
    const auto root = fs::absolute(inbox).lexically_normal();
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
        if (!scan.consume(0, 1, 1)) {
            issues.push_back({root.u8string(), "import entry limit reached"});
            break;
        }
        if (!scan.permits_depth(static_cast<std::size_t>(it.depth()) + 1)) {
            it.disable_recursion_pending();
            continue;
        }
        if (!fs::is_regular_file(it->symlink_status())) continue;
        auto ext = it->path().extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        if (ext == ".fbx") files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    const auto generated = generated_entries(catalog);
    for (const auto& file : files) {
        try {
            import_one(file, root, catalog, analyze, generated);
        } catch (const std::exception& error) {
            issues.push_back({file.u8string(), error.what()});
        }
    }
    return issues;
}

} // namespace ergo::studio::plugins::asset_import
