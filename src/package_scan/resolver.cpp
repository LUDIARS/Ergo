#include "resolver.h"

#include <cctype>

namespace ergo::package_scan::detail {

namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string last_segment(const std::string& path) {
    std::string s = path;
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    const auto cut = s.find_last_of("/\\");
    return cut == std::string::npos ? s : s.substr(cut + 1);
}

} // namespace

std::string Resolver::for_repo_name(const std::string& repo) const {
    const std::string key = lower(repo);
    for (const auto& o : manifest->observe) {
        if (is_safe_relative(o.path_hint) && lower(last_segment(o.path_hint)) == key) return o.name;
    }
    for (const auto& t : manifest->tools) {
        if (lower(t.name) == key || lower(last_segment(t.path)) == key) return t.name;
    }
    return "repo:" + repo;
}

std::string Resolver::for_submodule(const Submodule& submodule) const {
    const std::string repo = submodule.url.empty() ? last_segment(submodule.path)
                                                   : repo_name_from_url(submodule.url);
    return for_repo_name(repo);
}

std::string Resolver::for_path(const Source& source, const fs::path& abs) const {
    const fs::path& base = source.scope.base();
    for (const auto& sm : source.submodules) {
        if (is_safe_relative(sm.path) && is_within(normalized(base / fs::u8path(sm.path)), abs)) {
            return for_submodule(sm);
        }
    }
    for (const auto& t : manifest->tools) {
        if (is_safe_relative(t.path) && is_within(normalized(ergo_root / fs::u8path(t.path)), abs)) {
            return t.name;
        }
    }
    if (is_within(ergo_root, abs)) {
        for (const auto& ep : ergo_paths) {
            if (is_within(normalized(ergo_root / fs::u8path(ep)), abs)) return ep;
        }
        return "ergo";
    }
    if (root.empty() || !is_within(root, abs)) return {};
    for (const auto& o : manifest->observe) {
        if (is_safe_relative(o.path_hint) && is_within(normalized(root / fs::u8path(o.path_hint)), abs)) {
            return o.name;
        }
    }
    const fs::path rel = abs.lexically_relative(root);
    if (rel.empty() || rel.begin() == rel.end()) return {};
    return for_repo_name(generic_utf8(*rel.begin()));
}

std::string Resolver::for_dependency(const Source& source, const PackageDep& dep) const {
    if (dep.kind == DepSpecKind::File) {
        const std::string spec_path = file_spec_path(dep.spec);
        fs::path dir = source.scope.base();
        if (!source.package_dir.empty()) dir /= fs::u8path(source.package_dir);
        const std::string hit =
            spec_path.empty() ? std::string{} : for_path(source, normalized(dir / fs::u8path(spec_path)));
        if (!hit.empty()) return hit;
        return "npm:" + dep.name;
    }
    const auto it = npm_names.find(dep.name);
    return it != npm_names.end() ? it->second : "npm:" + dep.name;
}

} // namespace ergo::package_scan::detail
