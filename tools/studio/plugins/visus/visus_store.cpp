#include "visus_store.h"

#include "../common/file_io.h"
#include "../common/safe_path.h"

#include <algorithm>
#include <cmath>
#include <system_error>

namespace ergo::studio::plugins::visus {
namespace fs = std::filesystem;
namespace {

const char* const kSuffix = ".visus.json";

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool skipped_dir(const std::string& name) {
    return (!name.empty() && name[0] == '.') || name == "node_modules" || name == "build" || name == "dist";
}

std::string text_of(const JsonValue* v, const std::string& dflt) {
    if (!v || v->is_null()) return dflt;
    return v->is_string() ? v->s : to_json(*v);
}

std::optional<VisusSummary> summarize(const fs::path& root, const fs::path& file) {
    std::string err;
    const auto  text = read_text_file(file, err);
    if (!text) return std::nullopt;
    JsonValue obj;
    if (!ergo::common::jsonm::parse(*text, obj) || !obj.is_object()) return std::nullopt;
    std::error_code ec;
    VisusSummary s;
    s.rel_path = relative_slash(root, file);
    s.abs_path = file.u8string();
    s.name     = text_of(obj.find("name"), "");
    const JsonValue* geo = obj.find("geometry");
    s.geometry_kind      = text_of(geo && geo->is_object() ? geo->find("kind") : nullptr, "none");
    const JsonValue* mats = obj.find("materials");
    s.materials_count     = mats && mats->is_array() ? mats->a->size() : 0;
    s.bytes               = fs::file_size(file, ec);
    s.mtime               = iso_utc(fs::last_write_time(file, ec));
    return s;
}

void walk(const fs::path& root, const fs::path& dir, std::vector<VisusSummary>& out) {
    std::error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec) return;
    for (const auto& ent : it) {
        const std::string name = ent.path().filename().u8string();
        if (skipped_dir(name)) continue;
        std::error_code e2;
        if (ent.is_directory(e2)) {
            walk(root, ent.path(), out);
        } else if (ent.is_regular_file(e2) && ends_with(name, kSuffix)) {
            if (auto s = summarize(root, ent.path())) out.push_back(std::move(*s));
        }
    }
}

} // namespace

JsonValue to_json_value(const VisusSummary& s) {
    return jobj({{"rel_path", jstr(s.rel_path)}, {"abs_path", jstr(s.abs_path)}, {"name", jstr(s.name)},
                 {"geometry_kind", jstr(s.geometry_kind)},
                 {"materials_count", jnum(static_cast<double>(s.materials_count))},
                 {"bytes", jnum(static_cast<double>(s.bytes))}, {"mtime", jstr(s.mtime)}});
}

std::vector<fs::path> parse_roots(const std::string& raw) {
    std::vector<fs::path> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const auto end  = raw.find(';', start);
        std::string part = raw.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto b = part.find_first_not_of(" \t");
        if (b != std::string::npos) {
            part = part.substr(b, part.find_last_not_of(" \t") - b + 1);
            std::error_code ec;
            out.push_back(fs::absolute(fs::u8path(part), ec).lexically_normal());
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

VisusStore::VisusStore(std::vector<fs::path> roots) : roots_(std::move(roots)) {}

const fs::path* VisusStore::root_at(double index) const {
    if (!std::isfinite(index) || index < 0 || std::floor(index) != index) return nullptr;
    const auto i = static_cast<std::size_t>(index);
    return i < roots_.size() ? &roots_[i] : nullptr;
}

std::vector<VisusSummary> VisusStore::scan(const fs::path& root) const {
    std::vector<VisusSummary> out;
    walk(root, root, out);
    std::sort(out.begin(), out.end(), [](const VisusSummary& a, const VisusSummary& b) { return a.rel_path < b.rel_path; });
    return out;
}

VisusStore::Result VisusStore::load(double root_index, const std::string& rel) const {
    const fs::path* root = root_at(root_index);
    if (!root) return {false, 400, "invalid root index", {}};
    if (rel.empty()) return {false, 400, "missing path", {}};
    const auto full = resolve_within(*root, rel);
    if (!full) return {false, 400, "path outside root", {}};
    std::string err;
    auto text = read_text_file(*full, err);
    if (!text) return {false, 404, err, {}};
    return {true, 200, {}, std::move(*text)};
}

VisusStore::Result VisusStore::save(double root_index, const std::string& rel, const std::string& content) const {
    const fs::path* root = root_at(root_index);
    if (!root) return {false, 400, "invalid root index", {}};
    if (rel.empty()) return {false, 400, "missing path", {}};
    if (!ends_with(rel, kSuffix)) return {false, 400, "path must end with .visus.json", {}};
    const auto full = resolve_within(*root, rel);
    if (!full) return {false, 400, "path outside root", {}};
    // Syntax only: Pictor's schema is permissive.
    JsonValue parsed;
    if (!ergo::common::jsonm::parse(content, parsed)) return {false, 400, "not valid JSON", {}};
    std::string err;
    if (!write_text_file(*full, content, err)) return {false, 500, err, {}};
    return {true, 200, {}, {}};
}

} // namespace ergo::studio::plugins::visus
