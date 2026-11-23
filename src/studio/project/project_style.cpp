#include "ergo/studio/project/project_style.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ergo::studio::project {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxRead = 1u << 20;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains(const std::string& text, const char* word) { return text.find(word) != std::string::npos; }

/// A CMake line that brings in another project and names Pictor, Ergo or Lapilli.
bool pulls_in_ludiars(const std::string& cmake) {
    const std::string text = lower(cmake);
    if (contains(text, "ergo_packages")) return true;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        const bool brings_in = contains(line, "find_package") || contains(line, "add_subdirectory") ||
                               contains(line, "fetchcontent_declare") || contains(line, "fetchcontent_makeavailable");
        const bool names = contains(line, "pictor") || contains(line, "ergo") || contains(line, "lapilli");
        if (brings_in && names) return true;
    }
    return false;
}

bool lapilli_submodule(const FileProbe& probe) {
    if (probe.exists("lib/lapilli")) return true;
    const auto modules = probe.read(".gitmodules");
    return modules && contains(lower(*modules), "lib/lapilli");
}

} // namespace

bool DiskProbe::exists(const std::string& rel) const {
    std::error_code ec;
    return fs::exists(root_ / fs::u8path(rel), ec);
}

std::optional<std::string> DiskProbe::read(const std::string& rel) const {
    std::error_code ec;
    const fs::path p = root_ / fs::u8path(rel);
    if (!fs::is_regular_file(p, ec)) return std::nullopt;
    std::ifstream in(p, std::ios::binary);
    if (!in) return std::nullopt;
    std::string text(kMaxRead, '\0');
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(in.gcount()));
    return text;
}

ProjectStyle detect(const FileProbe& probe) {
    if (probe.exists("ProjectSettings/ProjectVersion.txt") && probe.exists("Assets")) return ProjectStyle::unity;
    const auto cmake = probe.read("CMakeLists.txt");
    if (!cmake) return ProjectStyle::other;
    return pulls_in_ludiars(*cmake) || lapilli_submodule(probe) ? ProjectStyle::cpp_ludiars : ProjectStyle::cpp_other;
}

std::string unity_editor_version(const FileProbe& probe) {
    const auto text = probe.read("ProjectSettings/ProjectVersion.txt");
    if (!text) return {};
    std::istringstream lines(*text);
    std::string line;
    const std::string key = "m_EditorVersion:";
    while (std::getline(lines, line)) {
        if (line.rfind(key, 0) != 0) continue;
        std::string v = line.substr(key.size());
        const auto first = v.find_first_not_of(" \t");
        const auto last  = v.find_last_not_of(" \t\r");
        return first == std::string::npos ? std::string() : v.substr(first, last - first + 1);
    }
    return {};
}

const char* style_name(ProjectStyle style) {
    switch (style) {
        case ProjectStyle::unity: return "unity";
        case ProjectStyle::cpp_ludiars: return "cpp-ludiars";
        case ProjectStyle::cpp_other: return "cpp-other";
        case ProjectStyle::other: break;
    }
    return "other";
}

std::optional<ProjectStyle> parse_style(const std::string& name) {
    for (ProjectStyle s : {ProjectStyle::unity, ProjectStyle::cpp_ludiars, ProjectStyle::cpp_other, ProjectStyle::other}) {
        if (name == style_name(s)) return s;
    }
    return std::nullopt;
}

} // namespace ergo::studio::project
