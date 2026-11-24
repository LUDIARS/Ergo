#include "capture_request.h"

#include "ergo/studio/platform/sha256.h"

#include <mutex>

namespace ergo::studio::plugins::asset_import {
namespace fs = std::filesystem;
namespace {
constexpr char kCaptureRevision[] = "thumbnail.sha256";
std::mutex capture_files;

bool has_capture_locked(const CatalogEntry& entry, const std::string& revision) {
    if (revision.empty()) return false;
    const auto stored = read_bounded(entry.directory / kCaptureRevision, 64);
    if (!stored || *stored != revision) return false;
    std::error_code ec;
    const auto status = fs::symlink_status(entry.directory / kThumbnailFile, ec);
    return !ec && fs::is_regular_file(status);
}
}

std::string asset_revision(const std::string& definition, const fs::path& source) {
    if (source.empty()) return platform::sha256_hex(definition);
    const auto bytes = read_bounded(source, kMaxModelBytes);
    return bytes ? platform::sha256_hex(platform::sha256_hex(definition) + platform::sha256_hex(*bytes))
                 : std::string{};
}

bool store_capture(IAssetCatalog& catalog, const CaptureRequest& request, const std::string& png) {
    if (!request.capture || request.revision.empty() || png.empty() || png.size() > kMaxThumbnailBytes) return false;
    const auto entry = catalog.find(request.name);
    if (entry.visus_path() != request.definition) return false;
    const auto bytes = read_bounded(request.definition, kMaxVisusBytes);
    if (!bytes || asset_revision(*bytes, request.source) != request.revision) return false;
    // Publish the revision only after the PNG exists.
    std::lock_guard<std::mutex> lock(capture_files);
    catalog.put(entry, kThumbnailFile, png);
    catalog.put(entry, kCaptureRevision, request.revision);
    return true;
}

bool has_capture(const CatalogEntry& entry, const std::string& revision) {
    std::lock_guard<std::mutex> lock(capture_files);
    return has_capture_locked(entry, revision);
}

std::optional<std::string> read_capture(const CatalogEntry& entry, const std::string& revision) {
    std::lock_guard<std::mutex> lock(capture_files);
    if (!has_capture_locked(entry, revision)) return std::nullopt;
    return read_bounded(entry.directory / kThumbnailFile, kMaxThumbnailBytes);
}

} // namespace ergo::studio::plugins::asset_import
