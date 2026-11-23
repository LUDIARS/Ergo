#pragma once
/// Read-only listing of a game's asset folders (the workspace profile's
/// "assets" entries). Pure apart from the filesystem reads: no network, no
/// writes. Paths from a request are resolved with resolve_within() so a
/// listing or a preview never leaves the configured folder.
#include "../common/json_util.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::assets {

/// What a file is, from its extension (model / texture / audio / shader /
/// scene / data / other). Lower-case comparison; ".scene.json" is a scene.
std::string kind_of(const std::filesystem::path& file);

/// MIME type for the preview route, or nullopt when the file is not
/// previewed (only raster images and small text files are).
std::optional<std::string> preview_type(const std::filesystem::path& file);

struct AssetEntry {
    std::string   name;       ///< file or folder name (UTF-8)
    std::string   path;       ///< relative to the asset root, "/" separated
    bool          directory = false;
    std::string   kind;       ///< "dir" for folders, else kind_of()
    std::uintmax_t size = 0;  ///< bytes (files only)
    std::int64_t  modified = 0;   ///< seconds since the Unix epoch (0 = unknown)
};

struct AssetListing {
    std::vector<AssetEntry> entries;   ///< folders first, then files, each by name
    bool                    truncated = false;
};

/// Hard cap of one listing (a folder with more entries is cut and flagged).
inline constexpr std::size_t kMaxListing = 2000;

/// One folder level of `root`/`rel`. nullopt (with `err`) when `rel` leaves
/// the root or is not a folder.
std::optional<AssetListing> list_folder(const std::filesystem::path& root, const std::string& rel, std::string& err);

/// The listing as JSON: {dir, truncated, entries: [{name, path, type, kind, size, modified}]}.
JsonValue listing_json(const std::string& rel, const AssetListing& listing);

/// Largest preview the route returns (images and text).
inline constexpr std::uintmax_t kMaxPreviewBytes = 16u * 1024u * 1024u;

} // namespace ergo::studio::plugins::assets
