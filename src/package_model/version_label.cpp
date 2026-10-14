#include "ergo/package_model/version_label.h"

#include <cctype>

namespace ergo::package_model {

namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// std::isdigit などは <locale> の多重定義と衝突し得るので関数ポインタで渡さない。
bool is_digit(unsigned char c) { return std::isdigit(c) != 0; }
bool is_hex(unsigned char c) { return std::isxdigit(c) != 0; }

bool all_of(const std::string& s, bool (*pred)(unsigned char)) {
    if (s.empty()) return false;
    for (const char c : s) {
        if (!pred(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

} // namespace

std::string version_from_describe(const std::string& raw) {
    std::string s = trim(raw);
    const std::string dirty = "-dirty";
    if (s.size() > dirty.size() && s.compare(s.size() - dirty.size(), dirty.size(), dirty) == 0) {
        s.resize(s.size() - dirty.size());
    }
    // <tag>-<count>-g<sha>
    const auto g = s.rfind("-g");
    if (g == std::string::npos || !all_of(s.substr(g + 2), is_hex)) return {};
    const auto dash = s.rfind('-', g == 0 ? 0 : g - 1);
    if (dash == std::string::npos || dash == 0) return {};
    const std::string count = s.substr(dash + 1, g - dash - 1);
    if (!all_of(count, is_digit)) return {};
    std::string tag = s.substr(0, dash);
    if (tag.size() > 1 && (tag[0] == 'v' || tag[0] == 'V') &&
        std::isdigit(static_cast<unsigned char>(tag[1]))) {
        tag.erase(0, 1);
    }
    return count == "0" ? tag : tag + "+" + count;
}

std::string version_from_revisor(const std::string& output) {
    for (std::size_t i = 0; i < output.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(output[i]))) continue;
        if (i > 0 && (std::isalnum(static_cast<unsigned char>(output[i - 1])) || output[i - 1] == '.')) {
            continue;
        }
        std::size_t j = i;
        int parts = 0;
        while (true) {
            const std::size_t start = j;
            while (j < output.size() && std::isdigit(static_cast<unsigned char>(output[j]))) ++j;
            if (j == start) break;
            ++parts;
            if (parts == 3 || j >= output.size() || output[j] != '.') break;
            ++j;
        }
        if (parts == 3) return output.substr(i, j - i);
    }
    return {};
}

std::string version_line(const VersionLabel& v) {
    if (v.text.empty()) return "版 不明";
    return v.source.empty() ? "版 " + v.text : "版 " + v.text + " [" + v.source + "]";
}

std::string short_sha(const std::string& sha) {
    return sha.size() > 7 ? sha.substr(0, 7) : sha;
}

} // namespace ergo::package_model
