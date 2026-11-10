#pragma once
/// SHA-256 (FIPS 180-4), incremental, for content hashes of downloaded and
/// local files (Curare's "sha256:<hex>").
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ergo::studio::platform {

class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t len);
    std::array<uint8_t, 32> finish();

private:
    void block(const uint8_t* p);

    uint32_t h_[8];
    uint8_t  buf_[64];
    std::size_t used_ = 0;
    uint64_t total_ = 0;
};

/// Lower-case hex of a digest.
std::string to_hex(const std::array<uint8_t, 32>& d);
std::string sha256_hex(const std::string& data);
/// Hex digest of a file's bytes; nullopt when it cannot be read.
std::optional<std::string> sha256_file_hex(const std::filesystem::path& file);

} // namespace ergo::studio::platform
