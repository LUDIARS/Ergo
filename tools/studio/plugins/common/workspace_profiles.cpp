#include "workspace_profiles.h"

#include "json_parse.h"
#include "json_util.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ergo::studio::plugins {
namespace fs = std::filesystem;

namespace {

/// "${NAME}" replaced by the environment's value; nullopt (with `err`) when one is unset.
std::optional<std::string> expand(const std::string& text, const ProfileEnvFn& env, std::string& err) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (text.compare(i, 2, "${") == 0) {
            const std::size_t close = text.find('}', i + 2);
            if (close == std::string::npos) {
                err = "unterminated ${ in \"" + text + "\"";
                return std::nullopt;
            }
            const std::string name = text.substr(i + 2, close - i - 2);
            const auto value = env ? env(name) : std::nullopt;
            if (!value || value->empty()) {
                err = "environment variable " + name + " is not set";
                return std::nullopt;
            }
            out += *value;
            i = close + 1;
            continue;
        }
        out += text[i++];
    }
    return out;
}

std::optional<fs::path> resolve(const fs::path& root, const std::string& text, const ProfileEnvFn& env, std::string& err) {
    const auto expanded = expand(text, env, err);
    if (!expanded) return std::nullopt;
    fs::path p = fs::u8path(*expanded);
    if (p.is_relative()) p = root / p;
    p = p.lexically_normal();
    // "<root>/." normalises to "<root>/": the same folder, written without the separator.
    if (p.has_parent_path() && p.filename().empty() && p != p.root_path()) p = p.parent_path();
    return p;
}

std::optional<std::string> read_text(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

void read_folder(const fs::path& dir, const std::string& suffix, const ProfileEnvFn& env, ProfileSet& out) {
    std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec)) return;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        const std::string name = entry.path().filename().u8string();
        if (entry.is_regular_file(ec) && name.size() > suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        const auto text = read_text(file);
        std::string err;
        if (!text) {
            out.errors.push_back(file.u8string() + ": cannot read");
            continue;
        }
        if (auto profile = parse_workspace_profile(*text, file, env, err)) {
            out.profiles.push_back(std::move(*profile));
        } else {
            out.errors.push_back(file.u8string() + ": " + err);
        }
    }
}

} // namespace

std::optional<WorkspaceProfile> parse_workspace_profile(const std::string& text, const fs::path& file,
                                                        const ProfileEnvFn& env, std::string& err) {
    const auto json = parse_json(text);
    if (!json || !json->is_object()) {
        err = "not a JSON object";
        return std::nullopt;
    }
    WorkspaceProfile p;
    p.source = file;
    const JsonValue* name = field(*json, "name");
    p.name = name != nullptr && name->is_string() ? name->as_string() : file.stem().u8string();
    p.root = file.parent_path();
    if (const JsonValue* root = field(*json, "root"); root != nullptr && root->is_string()) {
        const auto r = resolve(p.root, root->as_string(), env, err);
        if (!r) return std::nullopt;
        p.root = *r;
    }
    if (const JsonValue* list = field(*json, "services"); list != nullptr && list->is_array()) {
        for (const auto& item : *list->a) {
            if (item.is_string()) p.services.push_back(item.as_string());
        }
    }
    if (const JsonValue* map = field(*json, "tuning"); map != nullptr && map->is_object()) {
        for (const auto& [app, dir] : *map->o) {
            if (!dir.is_string()) continue;
            const auto d = resolve(p.root, dir.as_string(), env, err);
            if (!d) return std::nullopt;
            p.tuning[app] = *d;
        }
    }
    if (const JsonValue* list = field(*json, "git"); list != nullptr && list->is_array()) {
        for (const auto& item : *list->a) {
            if (!item.is_string()) continue;
            const auto d = resolve(p.root, item.as_string(), env, err);
            if (!d) return std::nullopt;
            p.git.push_back(*d);
        }
    }
    return p;
}

ProfileSet load_workspace_profiles(const fs::path& home, const std::string& plugin_dirs, const ProfileEnvFn& env) {
    ProfileSet out;
    if (!home.empty()) read_folder(home / "profiles", ".json", env, out);
    std::size_t start = 0;
    while (start <= plugin_dirs.size()) {
        const std::size_t end = plugin_dirs.find(';', start);
        const std::string dir = plugin_dirs.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty()) read_folder(fs::u8path(dir), ".studio.json", env, out);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

std::optional<std::string> process_env(const std::string& name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name.c_str()) != 0 || value == nullptr) return std::nullopt;
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name.c_str());
    return value != nullptr ? std::optional<std::string>(value) : std::nullopt;
#endif
}

} // namespace ergo::studio::plugins
