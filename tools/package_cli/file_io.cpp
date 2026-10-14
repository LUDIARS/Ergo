#include "file_io.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace ergo::package_cli {

namespace fs = std::filesystem;

std::optional<std::string> read_file(const std::string& path) {
    std::error_code ec;
    if (!fs::is_regular_file(fs::u8path(path), ec)) return std::nullopt;
    std::ifstream in(fs::u8path(path), std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool write_file(const std::string& path, const std::string& content) {
    std::error_code ec;
    const fs::path p = fs::u8path(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << content;
    return static_cast<bool>(out);
}

bool read_json(const std::string& path, jsonm::JsonValue& out, std::string* error) {
    const auto text = read_file(path);
    if (!text) {
        if (error) *error = path + ": not found";
        return false;
    }
    if (!jsonm::parse(*text, out)) {
        if (error) *error = path + ": not valid JSON";
        return false;
    }
    return true;
}

std::string join(const std::string& dir, const std::string& rel) {
    if (dir.empty()) return rel;
    const char last = dir.back();
    return (last == '/' || last == '\\') ? dir + rel : dir + "/" + rel;
}

} // namespace ergo::package_cli
