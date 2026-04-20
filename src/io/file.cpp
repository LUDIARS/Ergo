#include "ergo/io/file.h"
#include "ergo/io/path.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace ergo::io {

namespace fs = std::filesystem;

namespace {

fs::path to_fs(const std::string& p) {
    // Treat the input as UTF-8. std::u8path would be cleaner but got
    // deprecated in C++20 — and we target C++17 anyway. Plain fs::path
    // preserves bytes on POSIX and on recent MSVC when the input isn't
    // malformed.
    return fs::path(p);
}

} // namespace

// ---- read ----------------------------------------------------------------

bool read_file(const std::string& path, std::string& out) {
    out.clear();
    std::ifstream f(to_fs(path), std::ios::in | std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff len = f.tellg();
    if (len < 0) return false;
    out.resize(static_cast<size_t>(len));
    f.seekg(0, std::ios::beg);
    if (len > 0) f.read(out.data(), len);
    return static_cast<std::streamoff>(f.gcount()) == len;
}

bool read_file_bytes(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    std::ifstream f(to_fs(path), std::ios::in | std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff len = f.tellg();
    if (len < 0) return false;
    out.resize(static_cast<size_t>(len));
    f.seekg(0, std::ios::beg);
    if (len > 0) f.read(reinterpret_cast<char*>(out.data()), len);
    return static_cast<std::streamoff>(f.gcount()) == len;
}

// ---- write ---------------------------------------------------------------

bool write_file(const std::string& path, const std::string& content) {
    const std::string parent = path::parent_of(path);
    if (!parent.empty() && !ensure_directory(parent)) return false;
    std::ofstream f(to_fs(path), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (!content.empty()) f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(f);
}

bool write_file_bytes(const std::string& path, const std::vector<uint8_t>& bytes) {
    const std::string parent = path::parent_of(path);
    if (!parent.empty() && !ensure_directory(parent)) return false;
    std::ofstream f(to_fs(path), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (!bytes.empty()) {
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(f);
}

// ---- queries / filesystem ops -------------------------------------------

bool exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(to_fs(path), ec);
}

bool is_directory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(to_fs(path), ec);
}

bool ensure_directory(const std::string& path) {
    std::error_code ec;
    if (fs::exists(to_fs(path), ec)) {
        return fs::is_directory(to_fs(path), ec);
    }
    fs::create_directories(to_fs(path), ec);
    return !ec && fs::is_directory(to_fs(path), ec);
}

bool remove_file(const std::string& path) {
    std::error_code ec;
    if (!fs::exists(to_fs(path), ec)) return true;
    if (fs::is_directory(to_fs(path), ec)) return false;
    return fs::remove(to_fs(path), ec) && !ec;
}

} // namespace ergo::io

// -------------------------------------------------------------------------
// Path helpers
// -------------------------------------------------------------------------

namespace ergo::io::path {

namespace fs = std::filesystem;

std::string parent_of(const std::string& path) {
    fs::path p(path);
    return p.parent_path().string();
}

std::string join(const std::string& a, const std::string& b) {
    return (fs::path(a) / fs::path(b)).make_preferred().string();
}

std::string extension_of(const std::string& path) {
    return fs::path(path).extension().string();
}

std::string stem_of(const std::string& path) {
    return fs::path(path).stem().string();
}

} // namespace ergo::io::path
