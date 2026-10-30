#include "ergo/package_model/file_api_reader.h"

#include "json_access.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>
#include <vector>

namespace ergo::package_model {

namespace {

namespace fs = std::filesystem;
using detail::get_array;
using detail::get_object;
using detail::get_string;
using detail::jsonm::JsonValue;

/// Ergo の CMakeLists の project 名。 ergo_root を決めるのに使う。
constexpr const char* kErgoProject = "Ergo";

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

bool is_drive_path(const std::string& p) {
    return p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':';
}

bool same_char(char a, char b, bool fold) {
    if (!fold) return a == b;
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

int version_major(const JsonValue& v) {
    const auto* ver = get_object(v, "version");
    if (!ver) return -1;
    const auto* major = ver->find("major");
    return (major && major->is_number()) ? static_cast<int>(major->n) : -1;
}

std::string version_text(const JsonValue& v) {
    const auto* ver = get_object(v, "version");
    if (!ver) return "?";
    const auto* major = ver->find("major");
    const auto* minor = ver->find("minor");
    auto num = [](const JsonValue* n) {
        return (n && n->is_number()) ? std::to_string(static_cast<int>(n->n)) : std::string("?");
    };
    return num(major) + "." + num(minor);
}

/// 最新の index-*.json (名前に時刻が入るので名前順の最後)。
std::string latest_index(const fs::path& reply) {
    std::error_code ec;
    std::string best;
    for (fs::directory_iterator it(reply, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = detail::generic_utf8(it->path().filename());
        if (name.rfind("index-", 0) == 0 && name.size() > 5 &&
            name.compare(name.size() - 5, 5, ".json") == 0 && name > best) {
            best = name;
        }
    }
    return best;
}

/// index から codemodel の jsonFile を探す。 version が 2 でなければエラー。
bool find_codemodel_file(const JsonValue& index, std::string& file, std::string* error) {
    std::vector<const JsonValue*> candidates;
    if (const auto* reply = get_object(index, "reply")) {
        if (const auto* client = get_object(*reply, "client-ergo")) {
            if (const auto* query = get_object(*client, "query.json")) {
                if (const auto* responses = get_array(*query, "responses")) {
                    for (const auto& r : *responses) candidates.push_back(&r);
                }
            }
        }
    }
    if (candidates.empty()) {
        if (const auto* objects = get_array(index, "objects")) {
            for (const auto& r : *objects) candidates.push_back(&r);
        }
    }
    for (const auto* r : candidates) {
        if (!r->is_object() || get_string(*r, "kind") != "codemodel") continue;
        if (version_major(*r) != 2) {
            return fail(error, "File API: unsupported codemodel version " + version_text(*r) +
                                   " (expected 2.x)");
        }
        file = get_string(*r, "jsonFile");
        if (!file.empty()) return true;
    }
    return fail(error, "File API: no codemodel reply (configure with the package hub "
                       "so client-ergo is queried)");
}

const JsonValue* pick_configuration(const JsonValue& codemodel, const std::string& name) {
    const auto* configs = get_array(codemodel, "configurations");
    if (!configs || configs->empty()) return nullptr;
    if (name.empty()) return &configs->front();
    for (const auto& c : *configs) {
        if (get_string(c, "name") == name) return &c;
    }
    return nullptr;
}

std::size_t index_of(const JsonValue& v, const std::string& key, std::size_t fallback) {
    const auto* n = v.find(key);
    return (n && n->is_number() && n->n >= 0) ? static_cast<std::size_t>(n->n) : fallback;
}

bool read_target(const fs::path& reply, const JsonValue& entry, Target& out, std::string* error) {
    const std::string file = get_string(entry, "jsonFile");
    JsonValue t;
    if (!detail::parse_json_file(detail::generic_utf8(reply / fs::u8path(file)), t, error)) return false;
    out.id   = get_string(t, "id");
    out.name = get_string(t, "name");
    out.type = get_string(t, "type");
    if (const auto* sources = get_array(t, "sources")) {
        for (const auto& s : *sources) {
            const std::string path = get_string(s, "path");
            if (!path.empty()) out.sources.push_back(normalize_path(path));
        }
    }
    if (const auto* deps = get_array(t, "dependencies")) {
        for (const auto& d : *deps) {
            const std::string id = get_string(d, "id");
            if (!id.empty()) out.dependencies.push_back(id);
        }
    }
    std::sort(out.dependencies.begin(), out.dependencies.end());
    out.dependencies.erase(std::unique(out.dependencies.begin(), out.dependencies.end()),
                           out.dependencies.end());
    // 生成器が足すもの (ALL_BUILD / ZERO_CHECK …) と UTILITY はリンクではないので読まない。
    if (detail::get_bool(t, "isGeneratorProvided") || out.type == "UTILITY") out.type.clear();
    return true;
}

} // namespace

std::string normalize_path(const std::string& path, const std::string& base) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    fs::path fp = fs::u8path(p);
    if (!base.empty() && !(fp.is_absolute() || is_drive_path(p) || (!p.empty() && p[0] == '/'))) {
        std::string b = base;
        std::replace(b.begin(), b.end(), '\\', '/');
        fp = fs::u8path(b) / fp;
    }
    std::string out = detail::generic_utf8(fp.lexically_normal());
    while (out.size() > 1 && out.back() == '/' && !(out.size() == 3 && is_drive_path(out))) {
        out.pop_back();
    }
    if (out == ".") out.clear();
    return out;
}

bool path_within(const std::string& path, const std::string& root) {
    if (root.empty() || path.size() < root.size()) return false;
    const bool fold = is_drive_path(root) || is_drive_path(path);
    for (std::size_t i = 0; i < root.size(); ++i) {
        if (!same_char(path[i], root[i], fold)) return false;
    }
    return path.size() == root.size() || path[root.size()] == '/' || root.back() == '/';
}

bool read_codemodel(const std::string& build_dir, PackageModel& model, std::string* error,
                    const CodeModelOptions& options) {
    const fs::path reply = fs::u8path(build_dir) / ".cmake" / "api" / "v1" / "reply";
    const std::string index_name = latest_index(reply);
    if (index_name.empty()) {
        return fail(error, "File API: no reply index under " + detail::generic_utf8(reply) +
                               " (configure this build dir with cmake first; CMake < 3.27 answers the package"
                               " hub's query on the next run, so run cmake on it once more)");
    }
    JsonValue index;
    if (!detail::parse_json_file(detail::generic_utf8(reply / fs::u8path(index_name)), index, error)) return false;
    std::string codemodel_file;
    if (!find_codemodel_file(index, codemodel_file, error)) return false;

    JsonValue codemodel;
    if (!detail::parse_json_file(detail::generic_utf8(reply / fs::u8path(codemodel_file)), codemodel, error)) {
        return false;
    }
    if (version_major(codemodel) != 2) {
        return fail(error, "File API: unsupported codemodel version " + version_text(codemodel) +
                               " (expected 2.x)");
    }
    if (const auto* paths = get_object(codemodel, "paths")) {
        model.source_root = normalize_path(get_string(*paths, "source"));
        model.build_root  = normalize_path(get_string(*paths, "build"));
    }
    const auto* config = pick_configuration(codemodel, options.configuration);
    if (!config) {
        return fail(error, "File API: configuration '" + options.configuration + "' not found");
    }
    model.configuration = get_string(*config, "name");

    std::vector<std::string> dir_source;
    if (const auto* dirs = get_array(*config, "directories")) {
        for (const auto& d : *dirs) {
            dir_source.push_back(normalize_path(get_string(d, "source"), model.source_root));
        }
    }
    std::vector<std::string> project_name;
    model.ergo_root.clear();
    if (const auto* projects = get_array(*config, "projects")) {
        for (const auto& p : *projects) {
            project_name.push_back(get_string(p, "name"));
            if (project_name.back() == kErgoProject && model.ergo_root.empty()) {
                if (const auto* idx = get_array(p, "directoryIndexes")) {
                    if (!idx->empty() && idx->front().is_number()) {
                        const auto d = static_cast<std::size_t>(idx->front().n);
                        if (d < dir_source.size()) model.ergo_root = dir_source[d];
                    }
                }
            }
        }
    }

    model.targets.clear();
    if (const auto* targets = get_array(*config, "targets")) {
        for (const auto& entry : *targets) {
            Target t;
            if (!read_target(reply, entry, t, error)) return false;
            if (t.type.empty()) continue;
            const std::size_t d = index_of(entry, "directoryIndex", dir_source.size());
            const std::size_t p = index_of(entry, "projectIndex", project_name.size());
            if (d < dir_source.size()) t.source_dir = dir_source[d];
            if (p < project_name.size()) t.project = project_name[p];
            model.targets.push_back(std::move(t));
        }
    }
    std::sort(model.targets.begin(), model.targets.end(), [](const Target& a, const Target& b) {
        return a.name != b.name ? a.name < b.name : a.id < b.id;
    });
    model.has_codemodel = true;
    return true;
}

} // namespace ergo::package_model
