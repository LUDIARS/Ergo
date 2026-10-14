#include "ergo/package_scan/gitmodules_reader.h"

#include <sstream>

namespace ergo::package_scan {

namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

std::string unquote(std::string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
    return s;
}

/// `[submodule "x"]` なら名前、 他の節なら "" と in_submodule=false。
bool section_name(const std::string& line, std::string& name) {
    const std::string body = trim(line.substr(1, line.find(']') - 1));
    const std::string head = "submodule";
    if (body.compare(0, head.size(), head) != 0) return false;
    name = unquote(trim(body.substr(head.size())));
    return true;
}

} // namespace

std::vector<Submodule> parse_gitmodules(const std::string& text) {
    std::vector<Submodule> out;
    std::istringstream in(text);
    std::string raw;
    int line_no = 0;
    bool in_submodule = false;
    Submodule cur;
    auto flush = [&] {
        if (in_submodule && !cur.path.empty()) out.push_back(cur);
        cur = Submodule{};
    };
    while (std::getline(in, raw)) {
        ++line_no;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line[0] == '[') {
            flush();
            in_submodule = line.find(']') != std::string::npos && section_name(line, cur.name);
            continue;
        }
        if (!in_submodule) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key   = trim(line.substr(0, eq));
        const std::string value = unquote(trim(line.substr(eq + 1)));
        if (key == "path") {
            cur.path      = value;
            cur.path_line = line_no;
        } else if (key == "url") {
            cur.url      = value;
            cur.url_line = line_no;
        }
    }
    flush();
    return out;
}

std::string repo_name_from_url(const std::string& url) {
    std::string s = url;
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    const std::string ext = ".git";
    if (s.size() > ext.size() && s.compare(s.size() - ext.size(), ext.size(), ext) == 0) {
        s.resize(s.size() - ext.size());
    }
    const auto cut = s.find_last_of("/\\:");
    return cut == std::string::npos ? s : s.substr(cut + 1);
}

} // namespace ergo::package_scan
