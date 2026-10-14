#include "scope.h"

#include <fstream>
#include <sstream>

namespace ergo::package_scan::detail {

namespace fs = std::filesystem;

bool is_safe_relative(const std::string& rel) {
    if (rel.empty() || rel.find(':') != std::string::npos) return false;
    if (rel[0] == '/' || rel[0] == '\\') return false;
    std::size_t start = 0;
    while (start <= rel.size()) {
        const auto end = rel.find_first_of("/\\", start);
        const std::string seg =
            rel.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (seg == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

std::string generic_utf8(const fs::path& path) {
    const auto s = path.generic_u8string();
    return std::string(s.begin(), s.end());
}

fs::path normalized(const fs::path& path) {
    std::error_code ec;
    fs::path abs = fs::absolute(path, ec);
    if (ec) abs = path;
    return abs.lexically_normal();
}

bool is_within(const fs::path& base, const fs::path& path) {
    auto b = base.begin();
    auto p = path.begin();
    for (; b != base.end(); ++b, ++p) {
        if (b->empty()) continue;   // 末尾の区切りが作る空の要素
        if (p == path.end() || *b != *p) return false;
    }
    return true;
}

Scope::Scope(fs::path base, std::string display, std::vector<std::string>* files_read)
    : base_(normalized(base)), display_(std::move(display)), files_read_(files_read) {}

std::string Scope::display(const std::string& rel) const {
    if (display_.empty()) return rel;
    if (display_.back() == ':') return display_ + rel;
    return display_ + "/" + rel;
}

bool Scope::read(const std::string& rel, std::string& out) const {
    if (!is_safe_relative(rel)) return false;
    const fs::path full = (base_ / fs::u8path(rel)).lexically_normal();
    if (!is_within(base_, full)) return false;
    std::error_code ec;
    if (!fs::is_regular_file(full, ec)) return false;
    std::ifstream file(full, std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    if (files_read_) files_read_->push_back(display(rel));
    return true;
}

bool Scope::is_directory(const std::string& rel) const {
    if (!is_safe_relative(rel)) return false;
    const fs::path full = (base_ / fs::u8path(rel)).lexically_normal();
    std::error_code ec;
    return is_within(base_, full) && fs::is_directory(full, ec);
}

} // namespace ergo::package_scan::detail
