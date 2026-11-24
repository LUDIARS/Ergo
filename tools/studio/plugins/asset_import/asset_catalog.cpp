#include "asset_catalog.h"

#include "../common/file_io.h"

#include <lorica/checked_size.hpp>
#include <lorica/input_budget.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {
namespace fs = std::filesystem;
namespace {

bool component(const std::string& name) {
    return !name.empty() && name != "." && name != ".." && name.back() != '.' && name.back() != ' ' &&
           name.find_first_of("/\\:*?\"<>|") == std::string::npos &&
           std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; });
}

bool plain_path(const fs::path& path) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    return !fs::is_symlink(status) && (!ec || ec == std::errc::no_such_file_or_directory);
}

} // namespace

fs::path CatalogEntry::visus_path() const { return directory / fs::u8path(visus_filename()); }

std::optional<std::string> read_bounded(const fs::path& file, std::size_t max_bytes) {
    if (!plain_path(file)) return std::nullopt;
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return std::nullopt;
    const auto size = fs::file_size(file, ec);
    if (ec || size > max_bytes) return std::nullopt;
    const auto capacity = lorica::checked_add(static_cast<std::size_t>(size), 1);
    lorica::input_budget budget({max_bytes + 1, 1, 1, 1});
    if (!capacity || !budget.consume(*capacity, 1, 1)) return std::nullopt;
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return std::nullopt;
    if (in.peek() != std::char_traits<char>::eof() || in.bad()) return std::nullopt;
    return bytes;
}

FolderAssetCatalog::FolderAssetCatalog(fs::path root) : root_(std::move(root)) {
    if (!plain_path(root_)) throw std::runtime_error("invalid asset catalog folder");
}

CatalogEntry FolderAssetCatalog::find(const std::string& name) const {
    if (!component(name)) throw std::invalid_argument("invalid Visus name");
    auto dir = root_ / fs::u8path(name);
    if (!plain_path(dir)) throw std::runtime_error("invalid Visus folder");
    return {name, std::move(dir)};
}

std::vector<CatalogEntry> FolderAssetCatalog::entries() const {
    std::vector<CatalogEntry> entries;
    std::error_code ec;
    fs::directory_iterator it(root_, ec);
    if (ec == std::errc::no_such_file_or_directory) return entries;
    if (ec) throw fs::filesystem_error("read asset catalog", root_, ec);
    for (const auto& entry : it) {
        auto name = entry.path().filename().u8string();
        if (component(name) && fs::is_directory(entry.symlink_status())) {
            entries.push_back({std::move(name), entry.path()});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return entries;
}

void FolderAssetCatalog::put(const CatalogEntry& entry, const std::string& filename,
                             const std::string& bytes) {
    if (!component(filename)) throw std::invalid_argument("invalid asset filename");
    const auto dir = find(entry.name).directory;
    const auto target = dir / fs::u8path(filename);
    const auto pending = dir / fs::u8path(filename + ".tmp");
    if (!plain_path(target) || !plain_path(pending)) throw std::runtime_error("invalid catalog file");
    std::string error;
    if (!write_text_file(pending, bytes, error)) throw std::runtime_error(error);
    std::error_code ec;
    fs::rename(pending, target, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(pending, ignored);
        throw fs::filesystem_error("publish asset catalog file", target, ec);
    }
}

} // namespace ergo::studio::plugins::asset_import
