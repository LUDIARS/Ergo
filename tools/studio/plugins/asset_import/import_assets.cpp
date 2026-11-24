#include "import_assets.h"

#include "../common/safe_path.h"
#include "ergo/studio/platform/sha256.h"
#include "pictor/visus/visus_serializer.h"

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

pictor::VisusDesc definition(const CatalogEntry& entry, const fs::path& source, const fs::path& inbox,
                             const std::string& hash, ModelAnalysis analysis) {
    pictor::VisusDesc desc;
    desc.name = entry.name;
    desc.kind = pictor::VisusKind::MODEL;
    desc.asset = path_from(entry.directory, source);
    desc.parts = std::move(analysis.parts);
    const auto format = source_format(source);
    for (auto& part : desc.parts) {
        std::vector<std::pair<std::string, std::string>> textures;
        for (const auto& [key, value] : part.metadata) {
            if (key.compare(0, 8, "texture.") != 0) continue;
            if ((format == SourceFormat::Gltf || format == SourceFormat::Glb) &&
                value.as_string().compare(0, 15, "embedded:image/") == 0) continue;
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
    return desc;
}

struct Generated {
    CatalogEntry entry;
    std::string bytes;
    fs::path source;
    std::string hash;
    pictor::VisusValue::Array alternatives;
};

std::map<fs::path, Generated> generated_entries(IAssetCatalog& catalog) {
    std::map<fs::path, Generated> generated;
    for (const auto& entry : catalog.entries()) {
        auto bytes = read_bounded(entry.directory / kImportRecord, kMaxVisusBytes);
        pictor::VisusDesc desc;
        if (!bytes || !pictor::from_visus_json(*bytes, desc) || entry.name != desc.name) continue;
        const auto source = desc.metadata.get_string("import.source");
        const auto hash = desc.metadata.get_string("import.sha256");
        if (!source || !hash) continue;
        auto path = fs::u8path(*source);
        auto key = source_family_key(path);
        const auto* alternatives = desc.metadata.get_array("import.source_files");
        generated.emplace(std::move(key), Generated{entry, std::move(*bytes), std::move(path), *hash,
            alternatives ? *alternatives : pictor::VisusValue::Array{}});
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

void import_one(const SourceFamily& family, const fs::path& inbox, IAssetCatalog& catalog,
                const Analyze& analyze, const Generated* previous) {
    const auto& file = family.model;
    std::optional<std::string> current;
    if (previous) {
        current = read_bounded(previous->entry.visus_path(), kMaxVisusBytes);
        if (current && *current != previous->bytes) return;
        if (!current && fs::exists(previous->entry.visus_path())) {
            throw std::runtime_error("existing Visus cannot be read");
        }
    }
    const auto bytes = read_bounded(file, kMaxModelBytes);
    if (!bytes) throw std::runtime_error("model could not be read within the file size limit");
    const auto hash = platform::sha256_hex(*bytes);
    pictor::VisusValue::Array alternatives;
    for (const auto& path : family.alternatives) alternatives.emplace_back(path.generic_u8string());
    const bool unchanged = previous && previous->source == file && previous->hash == hash;
    if (unchanged && previous->alternatives == alternatives) {
        if (!current) catalog.put(previous->entry, previous->entry.visus_filename(), previous->bytes);
        return;
    }
    const auto entry = previous ? previous->entry : allocate(file, catalog);
    pictor::VisusDesc desc;
    if (unchanged) {
        if (!pictor::from_visus_json(previous->bytes, desc)) throw std::runtime_error("invalid import record");
    } else {
        desc = definition(entry, file, inbox, hash, analyze ? analyze(file, *bytes) : analyze_model(file, *bytes));
    }
    desc.metadata.set("import.source_files", std::move(alternatives));
    auto json = pictor::to_visus_json(desc);
    if (json.size() > kMaxVisusBytes) throw std::runtime_error("generated Visus exceeds file size limit");
    if (previous) {
        // Parsing can take time. Recheck the edited document immediately before publishing.
        const auto latest = read_bounded(entry.visus_path(), kMaxVisusBytes);
        if (latest ? *latest != previous->bytes : fs::exists(entry.visus_path())) return;
    }
    catalog.put(entry, entry.visus_filename(), json);
    catalog.put(entry, kImportRecord, json);
}

} // namespace

std::vector<ImportIssue> import_assets(const fs::path& inbox, IAssetCatalog& catalog, const Analyze& analyze) {
    std::vector<ImportIssue> issues;
    const auto root = fs::absolute(inbox).lexically_normal();
    const auto scan = scan_source_families(root);
    if (scan.entry_limit_reached) return {{root.u8string(), "import entry limit reached"}};
    const auto generated = generated_entries(catalog);
    for (const auto& family : scan.families) {
        try {
            // The background scan can reject an unsupported format without opening the model.
            if (!analyze) {
                if (const auto* issue = model_loader_issue(source_format(family.model))) {
                    issues.push_back({family.model.u8string(), issue});
                    continue;
                }
            }
            const auto previous = generated.find(source_family_key(family.model));
            import_one(family, root, catalog, analyze, previous == generated.end() ? nullptr : &previous->second);
        } catch (const std::exception& error) {
            issues.push_back({family.model.u8string(), error.what()});
        }
    }
    return issues;
}

} // namespace ergo::studio::plugins::asset_import
