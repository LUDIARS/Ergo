#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::asset_import {

struct CatalogEntry {
    std::string name;
    std::filesystem::path directory;

    std::string visus_filename() const { return name + ".visus.json"; }
    std::filesystem::path visus_path() const;
};

class IAssetCatalog {
public:
    virtual ~IAssetCatalog() = default;
    virtual std::vector<CatalogEntry> entries() const = 0;
    virtual CatalogEntry find(const std::string& name) const = 0;
    virtual void put(const CatalogEntry& entry, const std::string& filename,
                     const std::string& bytes) = 0;
};

class FolderAssetCatalog final : public IAssetCatalog {
public:
    explicit FolderAssetCatalog(std::filesystem::path root);
    std::vector<CatalogEntry> entries() const override;
    CatalogEntry find(const std::string& name) const override;
    void put(const CatalogEntry& entry, const std::string& filename,
             const std::string& bytes) override;

private:
    std::filesystem::path root_;
};

std::optional<std::string> read_bounded(const std::filesystem::path& file, std::size_t max_bytes);

} // namespace ergo::studio::plugins::asset_import
